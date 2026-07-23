# Animator Controller Editor

This document describes the design, user interface, and runtime synchronization of the **Animator Controller Editor** (`EditorUI::drawAnimatorControllerWindow()`), a Unity-like visual state machine editor built on top of the engine's generic [Node Graph Framework](node_graph_framework.md).

---

## 1. Overview & Core Purpose

The Animator Controller Editor provides a visual workflow for designing character animation state machines, locomotion blend trees, and state transitions. It operates directly on the active entity's `AnimationControllerComponent` and `AnimatorComponent`.

```
+-------------------+--------------------------------+-----------------------+
|  Parameters Panel |       Node Graph Canvas        |     Detail Panel      |
|     (140px)       |                                |        (260px)        |
|                   |  [Entry] (Green)               |                       |
|  - velocityX: 0.0 |     |                          |  Selected State:      |
|  - velocityY: 0.0 |     v                          |  - Name, Clip Picker  |
|  - speed:     0.0 |  [Idle] (Blue) ----> [Run]     |  - Speed & Loop       |
|                   |     ^               /          |  - Blend Tree Editor  |
|  + Add Param      |     |              v           |  - Transitions        |
|  + Add State      |  [Any State] (Purple)          |    & Conditions       |
|  + Add BlendTree  |                                |                       |
+-------------------+--------------------------------+-----------------------+

---

## Drag-and-Drop Asset Workflow

The editor supports intuitive drag-and-drop workflows directly from the **Asset Browser** panel (dragging `.anim`, `.gltf`, `.glb`, or `.fbx` files):

1. **Dragging onto the Grid Canvas**:
   - Dragging an animation file onto empty grid space automatically loads the animation clip and spawns a new `AnimationState` node at the dropped mouse position on the canvas.
2. **Dragging onto the Detail Panel**:
   - Dragging an animation file onto the right-side detail panel while a **Standard State** is selected assigns that clip to the state.
   - Dragging an animation file onto the detail panel while a **Blend Tree State** is selected appends a new `BlendNode` to the motion list.
3. **Dragging onto Clip Pickers**:
   - Dragging an animation file directly onto the Animation Clip combo or Motion List dropdown immediately maps the file to that slot.

```

---

## 2. Component Architecture & Data Model

The editor reads and writes to two primary ECS components on the selected entity:

### 1. `AnimatorComponent`
Holds the pool of loaded `AnimationClip` assets (`animator->animations`) available for assignment to states and blend trees.

### 2. `AnimationControllerComponent`
Defines the state machine graph structure:
```cpp
struct AnimationControllerComponent {
    std::vector<AnimationState> states;
    std::vector<AnimationTransition> transitions;
    std::string currentState;
    
    // Named parameters for transitions and blend trees (e.g., "speed", "velocityX")
    std::unordered_map<std::string, float> parameters;

    // Runtime crossfading & blending tracking
    std::string fromState;
    std::string targetState;
    float currentStateTime = 0.0f;
    float crossfadeProgress = 0.0f;
    float crossfadeDuration = 0.2f;
    bool isCrossfading = false;
};
```

---

## 3. Visual State Nodes

States are displayed as color-coded node cards on the graph canvas:

| Node Type | Header Color | Pins | Description |
|---|---|---|---|
| **Entry** | Green (`#288C3C`) | 1 Output (`Start`) | Initial entry point of the state machine. Automatically connects to the default starting state. |
| **Any State** | Purple (`#642882`) | 1 Output (`To`) | Global state transition origin. Transitions from Any State fire regardless of the active state. |
| **State** | Dark Blue (`#285096`) | 1 Input (`In`), 1 Output (`Out`) | Standard single-clip animation state. |
| **Blend Tree** | Teal (`#1E6E6E`) | 1 Input (`In`), 1 Output (`Out`) | Multi-clip blend state driven by controller parameters. |

---

## 4. Parameter Management (Left Panel)

The left side panel displays and edits the `parameters` map of the `AnimationControllerComponent`:

- **Adding Parameters**: Enter a parameter name (e.g. `speed`, `velocityX`) and click **+ Add Param**.
- **Interactive Tweaking**: Drag float values in real-time. In Play Mode, parameters can be driven dynamically by game scripts or the `PlayerControllerSystem` (e.g., WASD key movement vectors).
- **Removing Parameters**: Click the `[X]` button next to a parameter name.
- **Quick State Creation**: Buttons for **+ State** and **+ Blend Tree** create new nodes directly.
- **Active State Indicator**: Highlights the currently active runtime state in green text.

---

## 5. State Properties & Blend Tree Editor (Detail Panel)

Selecting any node card opens its properties in the right-side detail panel:

### Standard State Properties
- **State Name**: Editable text field. Renaming a state automatically updates all associated transitions.
- **Is Blend Tree**: Checkbox to convert a standard state into a Blend Tree state.
- **Animation Clip Picker**: Dropdown menu selecting an `AnimationClip` from `animator->animations`.
- **Playback Controls**: Sliders for playback speed (`speed`) and looping toggle (`isLooping`).

### 1D & 2D Blend Tree Editor
When editing a Blend Tree state, the detail panel provides a complete visual blend configuration:

1. **Blend Type Selection**: Toggle between **1D** and **2D** blending modes.
2. **Parameter Binding**:
   - **1D Mode**: Single parameter name field (e.g. `speed`).
   - **2D Mode**: Dual parameter name fields (e.g. `velocityX` for X-axis, `velocityY` for Y-axis).
3. **1D Visual Threshold Bar**:
   - Renders a interactive visual track showing the relative positions of all motion thresholds.
   - Orange triangle markers indicate where each animation clip reaches full weight ($1.0$).
4. **Motion List**:
   - **Clip Assignment**: Dropdown menu selecting an `AnimationClip` for each blend node.
   - **Threshold Values**:
     - 1D mode: Drag float threshold value (`Thr X.XX`).
     - 2D mode: Drag 2D coordinate thresholds (`X: 0.00`, `Y: 1.00`).
   - **Adding/Removing**: Click **+ Add Motion** or `-` to modify motion slots.

---

## 6. Transitions & Condition Rules

Creating a transition between two states is done visually by dragging from an output pin to an input pin.

### Transition Properties (in Detail Panel)
Each state node's detail panel lists all outgoing transitions originating from that state:

- **Destination Label**: Displays the target state name (`-> TargetState`).
- **Crossfade Duration**: Drag float controlling the crossfade blending time in seconds (e.g., `0.20s`).
- **Condition Rules Table**: Transitions trigger only when all attached conditions evaluate to true. Each condition specifies:
  1. **Parameter Name**: The parameter to evaluate (e.g. `speed`).
  2. **Operator**: Comparison dropdown (`>`, `<`, `==`).
  3. **Threshold Value**: Floating-point comparison value (e.g. `0.05`).
- **Adding/Removing Conditions**: Buttons for `+ Condition` and `X` per row.

---

## 7. Real-Time ECS Synchronization

The editor maintains two-way synchronization between the `NodeGraph` UI model and the ECS runtime component:

1. **Graph Reconstruction**: If the selected entity changes or `controller->states.size()` changes, the graph automatically rebuilds node positions, colors, and pin links.
2. **`onLinkCreated`**: Drawing a link between two state nodes appends a new `AnimationTransition { fromState, toState }` to the component.
3. **`onLinkDeleted`**: Deleting a Bezier link removes the corresponding `AnimationTransition`.
4. **`onNodeDeleted`**: Deleting a state node removes its `AnimationState` from `controller->states` and cleans up any dangling transitions referencing that state.
