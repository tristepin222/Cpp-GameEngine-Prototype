# Physics & Collision System

This document details the architecture and mathematical principles of the engine's 3D Physics System. The physics system handles integration, rigid body dynamics, multi-shape collision detection, contact point estimation, and rotational impulse resolution.

---

## Architecture Overview

The physics engine is designed as an ECS System (`PhysicsSystem`) which interacts with three main components:
1. **[Transform](../engine/include/ecs/components/Transform.hpp)**: Stores the absolute world position and Euler orientation (in degrees).
2. **[RigidBodyComponent](../engine/include/ecs/components/RigidBody.hpp)**: Holds mass, linear/angular velocity, forces, torques, restitution, and drag coefficients.
3. **[ColliderComponent](../engine/include/ecs/components/Collider.hpp)**: Defines the bounding volume shape (Sphere, AABB, or OBB) and shape properties.

---

## RigidBody Dynamics

The simulation separates rigid body dynamics into two phases: **Integration** (applying external forces and integrating state over time) and **Collision Resolution** (handling contact impulses).

### 1. Linear & Angular Integration
In the integration pass, forces and torques are integrated into velocities, and velocities are integrated into position/rotation coordinates:

$$\vec{a} = \frac{\vec{F}_{\text{total}}}{m}$$

$$\vec{v}_{t+dt} = \left(\vec{v}_t + \vec{a} \cdot dt\right) \cdot \text{clamp}(1 - C_{\text{linear\_drag}} \cdot dt, 0, 1)$$

$$\vec{p}_{t+dt} = \vec{p}_t + \vec{v}_{t+dt} \cdot dt$$

$$\vec{\alpha} = I^{-1} \vec{\tau}$$

$$\vec{\omega}_{t+dt} = \left(\vec{\omega}_t + \vec{\alpha} \cdot dt\right) \cdot \text{clamp}(1 - C_{\text{angular\_drag}} \cdot dt, 0, 1)$$

$$\vec{\theta}_{t+dt} = \vec{\theta}_t + \text{deg}(\vec{\omega}_{t+dt}) \cdot dt$$

Where:
* $I^{-1}$ is the inverse moment of inertia tensor in world coordinates.
* $\vec{\omega}$ is the angular velocity in radians per second.
* $\vec{\theta}$ is the Euler rotation in degrees (normalized to $[0, 360]$ via `fmod` to prevent floating-point drift).

---

## Collision Shapes

The engine supports three distinct collider shapes:

| Shape | Description | Rotation | Scaling |
| :--- | :--- | :--- | :--- |
| **Sphere** | Sphere bounding volume defined by local offset and radius. | None (rotation is ignored) | None (radius remains unscaled) |
| **AABB** | Axis-Aligned Bounding Box. | None (remains parallel to world axes) | None (extents are unscaled) |
| **OBB** | Oriented Bounding Box. | Rotates with the entity's hierarchy transform | Scaled by the world scale of the entity |

---

## Collision Detection Algorithms

The system routes collision queries inside **[PhysicsSystem.hpp](../engine/include/ecs/systems/PhysicsSystem.hpp)** using dedicated geometric solvers:

### 1. Sphere-Sphere
Checks if the distance between the world positions of the sphere centers is less than the sum of their radii:

$$\text{distance}(\vec{C}_A, \vec{C}_B) < r_A + r_B$$

### 2. Sphere-AABB
Finds the closest point on the AABB boundary to the sphere center by clamping coordinates to AABB bounds:

$$\vec{P}_{\text{closest}} = \text{clamp}(\vec{C}_{\text{sphere}}, \vec{C}_{\text{aabb}} - \vec{e}, \vec{C}_{\text{aabb}} + \vec{e})$$

If the distance between $\vec{C}_{\text{sphere}}$ and $\vec{P}_{\text{closest}}$ is less than the sphere's radius, a collision has occurred.

### 3. OBB-Sphere
Translates the sphere center $\vec{C}_s$ into the OBB's local coordinate space. In local space, the problem reduces to a Sphere-AABB check against local extents:

$$\vec{C}_{s,\text{local}} = \mathbf{R}^T \left(\vec{C}_s - \vec{C}_{\text{obb}}\right)$$

We clamp $\vec{C}_{s,\text{local}}$ to local extents, transform the point back to world space to obtain the contact point, and evaluate the distance check.

### 4. OBB-OBB (Separating Axis Theorem)
Applies the **Separating Axis Theorem (SAT)**. Two oriented boxes do not overlap if there exists a projection axis along which their 1D projections do not overlap. We project the boxes onto 15 candidate axes:
* 3 local axes of Box A ($\vec{u}_{A0}, \vec{u}_{A1}, \vec{u}_{A2}$)
* 3 local axes of Box B ($\vec{u}_{B0}, \vec{u}_{B1}, \vec{u}_{B2}$)
* 9 cross products of their axes ($\vec{u}_{Ai} \times \vec{u}_{Bj}$)

For each axis $\vec{L}$, we calculate the projected half-extents radii:

$$r_A = \sum_{i=0}^2 e_{A,i} \cdot |\vec{u}_{A,i} \cdot \vec{L}|, \quad r_B = \sum_{j=0}^2 e_{B,j} \cdot |\vec{u}_{B,j} \cdot \vec{L}|$$

If the center-to-center distance projected onto $\vec{L}$ satisfies $d = |(\vec{C}_B - \vec{C}_A) \cdot \vec{L}| \ge r_A + r_B$, the boxes are separated. If no separating axis is found, they are colliding along the axis of minimum penetration.

---

## Contact Point Estimation

For velocity resolution and rotational impulse calculations, the contact point is estimated at the interface:

* **Sphere-Sphere**: The midpoint along the line segment between centers.
* **Sphere-AABB / OBB-Sphere**: The closest point clamped on the box surface.
* **OBB-OBB**: The midpoint between the deepest support points of the two boxes along the contact normal:
  
  $$\vec{S}_A = \vec{C}_A + \sum_{i=0}^2 \text{sign}(\vec{u}_{A,i} \cdot \vec{N}) \cdot e_{A,i} \cdot \vec{u}_{A,i}$$
  
  $$\vec{S}_B = \vec{C}_B - \sum_{j=0}^2 \text{sign}(\vec{u}_{B,j} \cdot \vec{N}) \cdot e_{B,j} \cdot \vec{u}_{B,j}$$
  
  $$\vec{P}_{\text{contact}} = \frac{\vec{S}_A + \vec{S}_B}{2}$$

---

## Rotational Impulse Resolution

When rigid bodies collide, they apply equal and opposite impulses at the contact point. The linear and rotational responses are resolved using a 3D impulse solver.

### 1. Local & World Moments of Inertia
For a box of mass $m$ and extents $\vec{e}$, the inverse inertia tensor in local space is diagonal:

$$\mathbf{I}_{\text{local}}^{-1} = \text{diag}\left( \frac{3}{m(e_y^2 + e_z^2)}, \frac{3}{m(e_x^2 + e_z^2)}, \frac{3}{m(e_x^2 + e_y^2)} \right)$$

For a sphere of mass $m$ and radius $r$:

$$\mathbf{I}_{\text{local}}^{-1} = \text{diag}\left( \frac{5}{2mr^2}, \frac{5}{2mr^2}, \frac{5}{2mr^2} \right)$$

We transform the tensor into world coordinates using the entity's normalized rotation matrix $\mathbf{R}$:

$$\mathbf{I}_{\text{world}}^{-1} = \mathbf{R} \cdot \mathbf{I}_{\text{local}}^{-1} \cdot \mathbf{R}^T$$

### 2. Rotational Impulse Solver
The relative contact velocity at the contact point $\vec{P}$ is:

$$\vec{v}_{\text{rel}} = \left( \vec{v}_B + \vec{\omega}_B \times \vec{r}_B \right) - \left( \vec{v}_A + \vec{\omega}_A \times \vec{r}_A \right)$$

Where $\vec{r}_A = \vec{P} - \vec{C}_A$ and $\vec{r}_B = \vec{P} - \vec{C}_B$.

If the bodies are moving towards each other ($\vec{v}_{\text{rel}} \cdot \vec{N} < 0$), the impulse scalar $j$ is calculated:

$$j = \frac{-(1 + e)(\vec{v}_{\text{rel}} \cdot \vec{N})}{\frac{1}{m_A} + \frac{1}{m_B} + \left( \left[ \mathbf{I}_{A,\text{world}}^{-1} (\vec{r}_A \times \vec{N}) \right] \times \vec{r}_A \right) \cdot \vec{N} + \left( \left[ \mathbf{I}_{B,\text{world}}^{-1} (\vec{r}_B \times \vec{N}) \right] \times \vec{r}_B \right) \cdot \vec{N}}$$

Where $e$ is the bounciness restitution. The resulting impulse vector $\vec{J} = j \vec{N}$ is applied to update linear and angular velocities:

$$\vec{v}_A \leftarrow \vec{v}_A - \frac{\vec{J}}{m_A}, \quad \vec{\omega}_A \leftarrow \vec{\omega}_A - \mathbf{I}_{A,\text{world}}^{-1} (\vec{r}_A \times \vec{J})$$

$$\vec{v}_B \leftarrow \vec{v}_B + \frac{\vec{J}}{m_B}, \quad \vec{\omega}_B \leftarrow \vec{\omega}_B + \mathbf{I}_{B,\text{world}}^{-1} (\vec{r}_B \times \vec{J})$$

This impulse response ensures that boxes tilt, rotate, slide, and bounce realistically when hitting flat ground or inclined objects.
