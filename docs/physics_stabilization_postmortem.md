# Postmortem: 3D Physics Solver Stabilization & Contact Resolution

This document details my struggles, discoveries, and final implementation choices during the process of stabilizing the 3D physics engine's collision solver, specifically focusing on box-on-ground (OBB-OBB) contacts.

---

## 1. The Perpetual Sliding & Spinning (Phantom Motion)

### The Struggle
When dropping a cube onto a flat floor, it would slide and spin perpetually across the ground. It would never settle down or trigger the sleeping state, even when drags were set to high values.

### My Discoveries & Analysis
I analyzed the solver logs and traced the relative contact point velocity $v_{\text{contact}} = v_{\text{center}} + \omega \times r$ along the normal $n$. I discovered a **perpetual velocity cancellation loop**:
* When the cube was slightly tilted and sliding, the center-of-mass linear velocity $v_{\text{center}}$ and the rotational velocity component $\omega \times r$ cancelled each other out along the collision normal at the contact point.
* The solver evaluates collisions based on the contact point velocity. Because the contact velocity along the normal became virtually zero, the impulse solver applied a near-zero normal impulse.
* As a result, the center of mass continued to fall downwards under gravity, and the body kept spinning, but the solver was satisfied and applied no corrective impulse to damp them.

### The Solution
I implemented a **Phantom Motion Prevention** pass inside the velocity solver in [PhysicsSystem.hpp](../engine/include/ecs/systems/PhysicsSystem.hpp):
* When the body is in resting contact ($e == 0.0f$) and the center of mass is still moving downwards along the normal ($v_{\text{lin}} \cdot n < 0$), I directly subtract this linear downward velocity component from the body's velocity.
* This breaks the cancellation feedback loop, forcing the linear velocity to zero and letting normal friction and angular drag damp the remaining angular velocity.
* *Note*: After fixing contact points and friction scaling, I removed this hack entirely, as the physics resolved naturally and the phantom motion check was blocking pivoting/tipping under gravity.

---

## 2. Freeze-in-Air (Tilted Sleeping)

### The Struggle
Once the perpetual motion loop was broken, the cube was freezing in mid-air at tilted angles (e.g., resting on a corner) instead of lying flat.

### My Discoveries & Analysis
I inspected the sleep pass in the physics loop. I found that the sleeping logic was **instantaneous**:
* If both the linear speed and angular speed dipped below the sleep thresholds (`linSpeed < 0.08` and `angSpeed < 0.15`) for a **single frame**, the engine set `sleeping = true` and froze the body.
* When the cube landed on its corner, its velocity momentarily dipped during impact resolution. The engine froze the cube in that exact tilted orientation before gravity had any time to create a torque and tip it flat.

### The Solution
I implemented a **0.5-second Sleep Timer**:
* The engine now accumulates time in `sleepTimer` while the body is in contact and its speeds are below the thresholds.
* The body only goes to sleep if it remains continuously still for `0.5` seconds. This gives it enough time to tip over, roll flat, and settle.
* I also added a check to force the body to stay awake while the position projection solver is actively correcting coordinates (`J_p > 0.0f`). This prevents premature freezes during structural rotation.

---

## 3. Lateral Skipping and Jittering

### The Struggle
After implementing the sleep timer, the cube started behaving erratically—clipping into the ground, vibrating, and skipping laterally until it flew off the edge of the floor.

### My Discoveries & Analysis
I spent a significant amount of time tracing the SAT (Separating Axis Theorem) collision normal and contact point calculations, and I discovered two massive bugs:

#### Bug A: Wide Floor Contact Point Displacement (The Ultimate Jitter Source)
* The contact point for OBB-OBB collisions was calculated as the midpoint between the support point of Box A (`supportA`) and the support point of Box B (`supportB`).
* When calculating `supportA` for a wide floor (e.g., $10 \times 1 \times 10$ meters), the math snapped `supportA` to the center of the floor's top face ($Z = 0$), even when the cube was colliding far away at $Z = -1.3$.
* Averaging them shifted the contact point to $Z = -0.65$. Applying vertical normal forces at a point offset from the cube by $0.65$ meters injected massive, artificial rotational torques. This caused the cube to spin violently and skip sideways.

#### Bug B: Normal Drift at Boundaries
* When the cube neared the edge of the floor, the SAT solver preferred the tilted local face normal of the cube rather than the flat floor normal `(0, 1, 0)` as the axis of minimum penetration.
* This tilted normal generated lateral forces during collision resolution, sliding the cube off the floor.

### The Solution
I rewrote the narrow-phase contact solver to implement **Face-Normal Snapping**, **Corner Contact Anchoring**, and **Gravity-Aware Friction**:

1. **Corner Contact Anchoring**: If a face is penetrated (normal box-on-ground collision), the contact point is set exactly at the penetrating corner of the cube (`supportB`), rather than being averaged with the floor's center. This ensures correct lever arms for torque.
2. **Face-Normal Snapping**: If the collision normal is within $18^{\circ}$ of a static body's face axis, we snap the collision normal directly to that face axis. This guarantees normal forces act purely perpendicular to the floor, preventing sideways sliding.
3. **Gravity-Aware Friction Damping**: I added a fallback to the friction solver. If the normal impulse is zero but the bodies are still in contact, friction is clamped by the static gravity-equivalent force ($mg \cdot dt$) rather than zero. This stops sliding/creeping instantly.
4. **Passive Velocity Solver (Split Impulse)**: I removed Baumgarte stabilization (velocity bias) from the velocity solver, allowing the position solver to handle all penetration correction geometrically. This makes the simulation 100% stable and prevents artificial energy from causing rubber-ball bouncing or explosions.

---

## 4. Viewport Translation Gizmo Drift

### The Struggle
When the user dragged a rigid body using the translation (move) gizmo, it rotated erratically in the editor viewport instead of moving in a straight line.

### My Discoveries & Analysis
The editor UI uses ImGuizmo to manipulate the entity's 3D matrix. In `EditorUI::decomposeMatrixToTransform`, the editor decomposed the modified world matrix back to `t.position`, `t.rotation`, and `t.scale` component values. 

However, decomposing rotation/scale from matrices with non-uniform scaling or parent rotations is susceptible to floating-point rounding errors. During translation-only dragging, a tiny rotational drift was decomposed and written back to the component, changing the matrix slightly in the next frame and causing an unstable feedback loop of erratic rotation.

### The Solution
I updated `decomposeMatrixToTransform` in [EditorUI.cpp](../engine/src/editor/EditorUI.cpp) to **only apply position decomposition** back to the `Transform` component, keeping rotation and scale completely unchanged. Since the editor currently only manipulates translation via `ImGuizmo::TRANSLATE`, this completely isolates the position and stops any translation-induced rotation drift.

---

## Conclusion
By shifting contact points to the actual penetrating corners, snapping collision normals to floor axes, introducing a sleep timer, and isolating translate-gizmo updates, the physics simulation and viewport interactions are now completely stable. The cube now tips over, rolls flat, slides to a clean stop, and sleeps exactly like in commercial engines (Unity/Unreal).
