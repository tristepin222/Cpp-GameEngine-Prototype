# Skeletal Animation & Skinning Subsystem

This document details the mathematical framework, ECS data layout, and Vulkan pipeline integrations designed for the **Skeletal Animation and Vertex Skinning Pipeline** (Stage 3).

---

## 1. Linear Blend Skinning (LBS) Math

To animate a 3D mesh using a virtual skeleton, vertices must deform dynamically according to the positions and orientations of their underlying joints (bones). The engine implements **Linear Blend Skinning (LBS)**.

### Mathematical Formulation
For each vertex \(v\), we compute its animated position \(v'\) as a weighted linear combination of its bone influences (up to 4 bones):

\[v' = \sum_{i=0}^{3} w_i \cdot \left( M_{\text{joint}(i)} \cdot B_{\text{joint}(i)}^{-1} \right) \cdot v\]

Where:
*   \(v\): The original vertex position in bind pose (mesh local space).
*   \(w_i\): The weight influence of bone \(i\) on this vertex, satisfying \(\sum w_i = 1.0\).
*   \(B_{\text{joint}(i)}^{-1}\): The **Inverse Bind Matrix** of the joint. It transforms the vertex from mesh space back into the joint's local space at the time the mesh was bound to the skeleton.
*   \(M_{\text{joint}(i)}\): The current **Global Transformation Matrix** of the joint in world space, calculated dynamically via Forward Kinematics.
*   \(\left( M \cdot B^{-1} \right)\): The final **Joint Offset Matrix** (or Skinning Matrix). It maps vertices from their default bind pose into their current animated pose.

---

## 2. Keyframe Interpolation

Animation clips consist of keyframe channels representing Translation, Rotation, and Scale over time. Since frames do not always align with keyframe timestamps, we interpolate values dynamically:

### Translation & Scale: Linear Interpolation (LERP)
For position and scale vectors, we perform a standard linear interpolation between keyframe \(A\) and keyframe \(B\):

\[P(t) = (1 - \alpha) \cdot P_A + \alpha \cdot P_B\]

Where \(\alpha = \frac{t - t_A}{t_B - t_A}\) is the normalized interpolation factor.

### Rotation: Spherical Linear Interpolation (SLERP)
To avoid gimbal lock and ensure smooth, constant-velocity rotations, joints store orientations as **quaternions** and interpolate using SLERP:

\[R(t) = \text{slerp}(R_A, R_B, \alpha) = \frac{\sin((1 - \alpha)\theta)}{\sin\theta} \cdot R_A + \frac{\sin(\alpha\theta)}{\sin\theta} \cdot R_B\]

Where \(\cos\theta = R_A \cdot R_B\) is the dot product of the quaternions.

---

## 3. Forward Kinematics (FK) Hierarchy

Joint positions are hierarchical: moving a shoulder bone must automatically translate the elbow, wrist, and fingers. 

### Local-to-Global Updates
For each frame, the **Animation System** resolves local joint transforms from interpolated keyframes:

\[T_{\text{local}} = T_{\text{translation}} \cdot R_{\text{rotation}} \cdot S_{\text{scale}}\]

We then traverse the skeleton tree from root to leaf to calculate the global transformation \(M_{\text{joint}}\) for each node recursively:

\[M_{\text{joint}} = M_{\text{parent}} \cdot T_{\text{local}}\]

```mermaid
graph TD
    Root[Root Joint Matrix] -->|* Local| Shoulder[Shoulder Joint Matrix]
    Shoulder -->|* Local| Elbow[Elbow Joint Matrix]
    Elbow -->|* Local| Wrist[Wrist Joint Matrix]
    Wrist -->|* Local| Hand[Hand / Fingers Matrix]
```

---

## 4. Vulkan Shader Integration

Skinning is performed on the GPU inside the vertex shader to leverage hardware acceleration.

### Input Bindings
The vertex buffer is expanded to carry bone influence arrays:
*   `inBoneIDs`: `ivec4` containing the index offsets of the 4 influencing joints.
*   `inBoneWeights`: `vec4` containing the blending weights.

### Uniform Layout (Set 2)
The calculated offset matrices are bound as a descriptor set:
```glsl
layout(set = 2, binding = 0) uniform JointPalette {
    mat4 joints[256]; // Supports up to 256 bones per skinned mesh (16 KB limit compliant)
} palette;
```

---

## 5. Locomotion State Machines & 1D Blend Trees

To implement complex movement animations (like walking, running, and turning), the engine features a locomotion state machine and pose-blending system.

### Locomotion State Machine
Managed by the `AnimationControllerComponent`, the state machine tracks:
*   **States**: Defined animation clips with custom speed coefficients.
*   **Parameters**: Input floats (e.g. `speed`, `direction`) evaluated to trigger transitions.
*   **Transitions**: Directed state connections containing condition arrays (e.g., transition from `Idle` to `Walk` if `speed > 0.1`).
*   **Crossfading**: Smooth linear interpolation (LERP) of bone translations/scales and spherical linear interpolation (SLERP) of bone orientations between the outgoing and incoming states over a configurable duration (e.g., `0.3` seconds).

### 1D Blend Trees
States can point to 1D Blend Trees, which blend multiple keyframe clips together based on an input parameter. For example:
*   A `Movement` state contains clips for `Walk` (threshold `0.2`) and `Run` (threshold `0.8`).
*   If `speed` is `0.5`, the system samples keyframes from both clips at the same timeline offset and interpolates their joint transforms linearly using the blending weight \(\beta = \frac{0.5 - 0.2}{0.8 - 0.2} = 0.5\).

---

## 6. Inverse Kinematics (IK) Solvers

While Forward Kinematics (FK) computes joint locations from parent to child, **Inverse Kinematics (IK)** computes joint rotations backwards to place an end-effector (e.g., foot, hand) exactly at a target position. The engine supports two solver types in the `IKSolverComponent`:

### 2-Bone Analytical Solver (Law of Cosines)
Used for simple limb joints (like thigh-shin-foot or shoulder-elbow-wrist). Given bone lengths \(a\) and \(b\), and distance to target \(c\):
1.  We calculate the interior angles of the triangle formed by the joint chain using the Law of Cosines:
    \[\cos\theta_{\text{knee}} = \frac{a^2 + b^2 - c^2}{2ab}\]
2.  We rotate the middle joint by \(\theta_{\text{knee}}\) relative to the hinge axis.
3.  We calculate the orientation of the start joint to align the end joint exactly with the target position.
4.  A world-space **pole vector** controls the bend direction of the hinge joint.

### Multi-Joint Iterative FABRIK Solver
The Forward And Backward Reaching Inverse Kinematics (FABRIK) solver resolves arbitrary joint chains (spines, tails, arms). It works in two passes:
1.  **Backward Pass**: Sets the tip joint position to the target, then pulls each preceding joint along the bone segment line to maintain constant bone lengths.
2.  **Forward Pass**: Sets the base joint back to its origin, and pushes each subsequent joint along the segment line.
3.  **Orientation Solver**: Once positions are solved, a sequential Forward Kinematics pass calculates the rotation quaternions between the original bone directions and the solved directions, propagating rotations down the chain. This preserves bone lengths and structural continuity.

---

## 7. Entity Transform Hierarchy & Skeletal Sharing

To support complex glTF models made of multiple submeshes (like a character with separate clothes or weapon parts), the ECS manages relationships using a parent-child transform hierarchy:

### Multi-Mesh Splitting
During model import, the engine splits multi-primitive glTF assets into separate child entities in the ECS parented to the first part root node using `HierarchyComponent`.

### Skeletal Sharing
To prevent duplicate bone math and GPU upload overhead:
*   Only the parent root entity holds the `SkeletonComponent` and `AnimatorComponent`.
*   During rendering, `RenderSystem` detects child entities. If they don't have a skeleton, it binds the parent root's joint matrices descriptor set (Set 2).
*   For skinned child meshes, the GPU vertex shader deforms vertices using this shared joint palette.
*   For rigid (non-skinned) child attachments, `RenderSystem::getWorldMatrix` searches for a bone matching the child's `parentBoneName` or `nodeName` in the parent's skeleton and multiplies the child's world matrix by the animated bone matrix.

### Recursive Deletion
When destroying a parent entity via the editor hierarchy panel, `Scene::deleteEntity` recursively gathers and deletes all child and grandchild descendants in the hierarchy tree to prevent memory leaks and orphaned entities in the registry.

---

## Case Study & Engineering Post-Mortem

To read about the real-world bugs, rendering constraints, and architectural challenges solved during the development of these systems, please refer to:
*   **[Skinned Animation Case Study (Post-Mortem)](file:///f:/GitHub/Cpp-GameEngine-Prototype/docs/skinned_animation_postmortem.md)**
