# Generic Node Graph Framework

This document describes the architectural design, rendering pipeline, serialization, and extensibility of the generic **Node Graph Framework** (`NodeGraphFramework.hpp` / `.cpp`).

---

## 1. Overview & Architectural Goals

The Node Graph Framework provides a reusable, highly extensible foundation for visual node-based editors within the engine. It is designed to power a wide range of toolsets, including:
- **Animator Controller State Machines** (locomotion state graphs, blend trees, transitions)
- **Shader Editors** (material node graphs, PBR nodes)
- **Visual Scripting Systems** (logic execution flow, event graphs)
- **Dialogue & Quest Tree Editors** (branching dialogue, choices, game flags)

### Core Design Principles
1. **Lightweight Node Cards**: Node cards on the canvas display only essential identification (title, type badge, pin slots). Detailed properties and parameters live in the framework's built-in detail panel.
2. **Framework-Level Detail Panel**: Built into the graph renderer (`NodeGraph::draw()`), a dockable right-side panel automatically renders rich property editors when a node is selected, powered by type-registered callbacks.
3. **Decoupled User Data**: Nodes carry an opaque `userData` pointer (`void*`) and custom string data payload (`customData`), allowing host systems to map nodes directly to underlying runtime data structures without modifying framework code.
4. **Type-Safe Pin Connections**: Connection slots enforce validation based on `NodePinType` identifiers and slot directions (Output $\rightarrow$ Input).

---

## 2. Core Data Structures

The framework is defined in namespace `Engine` within [`engine/include/editor/NodeGraphFramework.hpp`](file:///f:/GitHub/Cpp-GameEngine-Prototype/engine/include/editor/NodeGraphFramework.hpp):

### NodePinType
Defines a pin's type name and UI slot color:
```cpp
struct NodePinType {
    std::string name; // e.g. "float", "int", "bool", "Vec3", "dialogue", "state"
    ImU32 color;      // IM_COL32 color used for pin circle and Bezier curve rendering
};
```
Built-in pin types include `PIN_TYPE_FLOW`, `PIN_TYPE_FLOAT`, `PIN_TYPE_INT`, `PIN_TYPE_BOOL`, `PIN_TYPE_STRING`, `PIN_TYPE_VEC3`, `PIN_TYPE_DIALOGUE`, and `PIN_TYPE_STATE`.

### NodePin
Represents an input or output connection slot on a node:
```cpp
struct NodePin {
    uint32_t id = 0;
    std::string name;
    NodePinType type;
    bool isOutput = false;
    uint32_t nodeId = 0;

    // Inline value fallback storage when unconnected
    float floatVal = 0.0f;
    int intVal = 0;
    bool boolVal = false;
    std::string stringVal;
};
```

### Node
Represents a visual node card in the graph canvas:
```cpp
struct Node {
    uint32_t id = 0;
    std::string title;
    std::string typeName;
    ImVec2 position{0.0f, 0.0f};
    ImVec2 size{0.0f, 0.0f};
    std::vector<NodePin> inputs;
    std::vector<NodePin> outputs;

    std::string customData;                                // String payload
    std::function<void(Node&)> customWidgetCallback;      // Inline card widget (optional)
    ImU32 headerColor = 0;                                 // Header background color override (0 = auto)
    void* userData = nullptr;                              // Opaque domain data pointer (e.g. AnimationState*)
};
```

### NodeLink
Represents a directed link connecting an output pin to an input pin:
```cpp
struct NodeLink {
    uint32_t id = 0;
    uint32_t fromPinId = 0; // Output pin ID
    uint32_t toPinId = 0;   // Input pin ID
};
```

---

## 3. Node Type Registry & Factory System

Host editors register node types into the `NodeGraph` instance using `registerNodeType()`. This populates the right-click creation menu and binds property rendering callbacks:

```cpp
struct NodeRegistryEntry {
    std::string category;
    std::string typeName;
    std::string title;

    // Called when spawning the node to add input/output pins
    std::function<void(uint32_t nodeId)> populateCallback;

    // Lightweight inline widget drawn on the card body (optional)
    std::function<void(Node& node)> customWidgetCallback;

    // Drawn in the framework's right-side detail panel when selected
    std::function<void(Node& node)> detailPanelCallback;
};
```

### Registering a Custom Node Type Example
```cpp
graph.registerNodeType("Dialogue", "DialogueSpeech", "Speech Node",
    // 1. Populate pins
    [&graph, pinDialogue](uint32_t nodeId) {
        graph.addInputPin(nodeId, "Prev", pinDialogue);
        graph.addOutputPin(nodeId, "Next", pinDialogue);
    },
    // 2. Inline widget (optional, keep minimal)
    nullptr,
    // 3. Detail panel callback (renders in right-side detail panel when selected)
    [](Engine::Node& nodeRef) {
        ImGui::TextDisabled("Speech Text:");
        char buf[256] = "";
        strncpy_s(buf, nodeRef.customData.c_str(), sizeof(buf) - 1);
        if (ImGui::InputTextMultiline("##speechTxt", buf, sizeof(buf), ImVec2(0, 80))) {
            nodeRef.customData = buf;
        }
    }
);
```

---

## 4. Canvas Rendering Pipeline & Viewport Interaction

The rendering entry point is `NodeGraph::draw()`:

```cpp
void NodeGraph::draw(const char* editorId, 
                    const ImVec2& canvasSize = ImVec2(0.0f, 0.0f), 
                    float detailPanelWidth = 220.0f);
```

### Rendering Layout
When `detailPanelWidth > 0`, `draw()` automatically splits the available region into a two-column layout:
```
+------------------------------------------+-----------------------+
|                                          |                       |
|           Canvas Area (ImGui Child)      |     Detail Panel      |
|                                          |     (220px Child)     |
|   - Infinite Grid                        |                       |
|   - Panning & Zoom                       |  Renders              |
|   - Bezier Curve Links                   |  detailPanelCallback  |
|   - Node Cards & Hitboxes                |  for selected node    |
|   - Pin Connections                      |                       |
|                                          |                       |
+------------------------------------------+-----------------------+
```

### Canvas Features
1. **Infinite Grid Background**: Draws a 64px grid pattern relative to the current canvas panning vector (`m_pan`).
2. **Canvas Panning**: Middle-click or right-click drag pans the canvas infinitely.
3. **Cubic Bezier Links**: Links are rendered as cubic Bezier curves (`ImDrawList::AddBezierCubic`) matching the color of the output pin type:
   \[P_1 = \text{pinOutPos}, \quad P_2 = P_1 + (60, 0), \quad P_3 = P_4 - (60, 0), \quad P_4 = \text{pinInPos}\]
4. **Drag-to-Connect Preview**: Dragging from an output pin renders a live curve attached to the mouse cursor until released over a compatible input pin.
5. **Interactive Hitbox & Overlap Handling**: Card bodies render with rounded rectangle borders and selection highlights. Delete buttons (`[x]`) and pin slots are drawn before the main hitbox to ensure input priority without click-swallowing.
6. **Right-Click Context Menu**: Right-clicking empty canvas space opens a categorized menu generated from `getRegisteredNodeTypes()`.

---

## 5. Event Callbacks & Lifecycle Integration

Host editors can register event handlers on the `NodeGraph` instance to synchronize runtime data structures when visual edits occur:

```cpp
// Fired when the user draws a new link between output and input pins
std::function<void(uint32_t fromPinId, uint32_t toPinId)> onLinkCreated;

// Fired when a link is destroyed
std::function<void(uint32_t fromPinId, uint32_t toPinId)> onLinkDeleted;

// Fired when a node is selected (nodeId = 0 on deselect)
std::function<void(uint32_t nodeId)> onNodeSelected;

// Fired after a node and its attached links are deleted
std::function<void(uint32_t nodeId)> onNodeDeleted;
```

---

## 6. JSON Serialization

The framework includes a built-in JSON scanner and formatter for node graphs.

### Export Format (`serialize()`)
```json
{
  "nextId": 5,
  "nodes": [
    {
      "id": 1,
      "title": "Speech Node",
      "typeName": "DialogueSpeech",
      "x": 50.0,
      "y": 150.0,
      "headerColor": 0,
      "customData": "Hello adventurer!",
      "inputs": [ { "id": 2, "name": "Prev", "type": "dialogue", "color": 4290772168, "fVal": 0.0, "iVal": 0, "bVal": 0, "sVal": "" } ],
      "outputs": [ { "id": 3, "name": "Next", "type": "dialogue", "color": 4290772168, "fVal": 0.0, "iVal": 0, "bVal": 0, "sVal": "" } ]
    }
  ],
  "links": [
    { "id": 4, "from": 3, "to": 2 }
  ]
}
```

### Import Format (`deserialize()`)
- Clears active graph memory and restores node positions, pins, custom data, and links.
- Automatically re-binds `detailPanelCallback` and `customWidgetCallback` by matching `typeName` against the `NodeGraph`'s registered node types.

---

## 7. How to Build a New Node Editor

To create a new node-based tool using this framework:

1. **Instantiate `Engine::NodeGraph`** as a member or static object in your editor panel.
2. **Register Node Types** (`registerNodeType`) during initialization, specifying input/output pins and `detailPanelCallback` UI layouts.
3. **Bind Event Callbacks** (`onLinkCreated`, `onLinkDeleted`, `onNodeDeleted`) to keep your domain data model in sync.
4. **Call `graph.draw("MyEditorId", size, detailPanelWidth)`** inside your ImGui window function.
