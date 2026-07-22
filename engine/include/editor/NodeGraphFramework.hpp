#pragma once
#include <string>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <imgui.h>

namespace Engine {

    /**
     * @struct NodePinType
     * @brief Defines the type name and connection slot color for validation and aesthetics.
     */
    struct NodePinType {
        std::string name; // e.g. "float", "int", "bool", "Vec3", "Dialogue", "state"
        ImU32 color = IM_COL32(200, 200, 200, 255);
    };

    /**
     * @struct NodePin
     * @brief Defines an input or output connection pin on a node.
     */
    struct NodePin {
        uint32_t id = 0;
        std::string name;
        NodePinType type;
        bool isOutput = false;
        uint32_t nodeId = 0;

        // Inline value storage (useful if input pin is not connected to a link)
        float floatVal = 0.0f;
        int intVal = 0;
        bool boolVal = false;
        std::string stringVal;
    };

    /**
     * @struct Node
     * @brief Defines a node card in the graph.
     */
    struct Node {
        uint32_t id = 0;
        std::string title;
        std::string typeName;
        ImVec2 position{0.0f, 0.0f};
        ImVec2 size{0.0f, 0.0f};
        std::vector<NodePin> inputs;
        std::vector<NodePin> outputs;

        // Custom data payload for user extensions (e.g. Dialogue text, script code)
        std::string customData;

        // Optional inline rendering callback for widgets drawn inside the node card body.
        // Keep this lightweight — heavy configuration belongs in detailPanelCallback.
        std::function<void(Node&)> customWidgetCallback = nullptr;

        // Per-node header color override (0 = use typeName heuristic fallback).
        ImU32 headerColor = 0;

        // Opaque user data pointer (e.g. index into controller.states[], not owned).
        void* userData = nullptr;
    };

    /**
     * @struct NodeLink
     * @brief Defines a connection between an output pin and an input pin.
     */
    struct NodeLink {
        uint32_t id = 0;
        uint32_t fromPinId = 0; // Output pin ID
        uint32_t toPinId = 0;   // Input pin ID
    };

    /**
     * @class NodeGraph
     * @brief Generic node-editor graph data structure, renderer, and serializer.
     *
     * Built-in features available to all graph editors:
     *  - Pannable infinite grid canvas
     *  - Bezier link drawing and drag-to-connect
     *  - Right-click node creation menu (registered types + categories)
     *  - Detail panel: right-side panel that shows detailPanelCallback of the selected node
     */
    class NodeGraph {
    public:
        NodeGraph();
        ~NodeGraph();

        // -----------------------------------------------------------------------
        // Node / Pin / Link editing API (ID-based, safe against vector reallocs)
        // -----------------------------------------------------------------------
        uint32_t createNode(const std::string& title, const std::string& typeName, ImVec2 pos = ImVec2(100.0f, 100.0f));
        uint32_t addInputPin(uint32_t nodeId, const std::string& name, const NodePinType& type);
        uint32_t addOutputPin(uint32_t nodeId, const std::string& name, const NodePinType& type);

        void addLink(uint32_t fromPinId, uint32_t toPinId);
        void removeLink(uint32_t linkId);
        void deleteNode(uint32_t nodeId);
        void clear();

        // -----------------------------------------------------------------------
        // Getters and Search Helpers
        // -----------------------------------------------------------------------
        Node* findNode(uint32_t nodeId);
        NodePin* findPin(uint32_t pinId);
        NodeLink* findLink(uint32_t linkId);
        NodeLink* findLinkConnectingToInput(uint32_t inputPinId);

        const std::vector<Node>&     getNodes() const { return m_nodes; }
        const std::vector<NodeLink>& getLinks() const { return m_links; }

        uint32_t getSelectedNodeId() const { return m_selectedNodeId; }
        void     setSelectedNodeId(uint32_t id) { m_selectedNodeId = id; }

        /** Convenience helper to change a node's header color after creation. */
        void setNodeHeaderColor(uint32_t nodeId, ImU32 color);

        // -----------------------------------------------------------------------
        // Serialization
        // -----------------------------------------------------------------------
        std::string serialize();
        bool deserialize(const std::string& jsonStr);

        // -----------------------------------------------------------------------
        // Rendering API
        // -----------------------------------------------------------------------

        /**
         * @brief Draw the node graph editor.
         *
         * @param editorId          Unique ImGui ID string for this graph instance.
         * @param canvasSize        Total area (canvas + detail panel). 0 = fill available.
         * @param detailPanelWidth  Width of the built-in right-side detail panel.
         *                          Pass 0.0f to disable the detail panel entirely.
         */
        void draw(const char* editorId,
                  const ImVec2& canvasSize = ImVec2(0.0f, 0.0f),
                  float detailPanelWidth = 220.0f);

        // -----------------------------------------------------------------------
        // Node Type Registry — for right-click creation menu + callback rebinding
        // -----------------------------------------------------------------------
        struct NodeRegistryEntry {
            std::string category;
            std::string typeName;
            std::string title;

            /** Called when the node is spawned to add its pins. */
            std::function<void(uint32_t)> populateCallback;

            /** Lightweight inline widget drawn inside the node card (keep minimal). */
            std::function<void(Node&)> customWidgetCallback;

            /**
             * @brief Drawn in the framework's built-in detail panel when this node is selected.
             * This is where all configuration UI lives — clip pickers, conditions, parameters, etc.
             */
            std::function<void(Node&)> detailPanelCallback;
        };

        void registerNodeType(const std::string& category,
                              const std::string& typeName,
                              const std::string& title,
                              std::function<void(uint32_t)> populateCallback,
                              std::function<void(Node&)> customWidgetCallback = nullptr,
                              std::function<void(Node&)> detailPanelCallback = nullptr);

        const std::vector<NodeRegistryEntry>& getRegisteredNodeTypes() const { return m_registeredTypes; }

        // -----------------------------------------------------------------------
        // Framework-level event callbacks (optional, set by the host editor)
        // -----------------------------------------------------------------------

        /** Fired after a node and all its links are destroyed. */
        std::function<void(uint32_t nodeId)> onNodeDeleted;

        /** Fired after a link is successfully created. */
        std::function<void(uint32_t fromPinId, uint32_t toPinId)> onLinkCreated;

        /** Fired just before a link is destroyed (passes original pin IDs). */
        std::function<void(uint32_t fromPinId, uint32_t toPinId)> onLinkDeleted;

        /** Fired when the user clicks / selects a node (nodeId = 0 means deselect). */
        std::function<void(uint32_t nodeId)> onNodeSelected;

    private:
        uint32_t generateId();

        uint32_t m_nextId = 1;
        std::vector<Node> m_nodes;
        std::vector<NodeLink> m_links;
        std::vector<NodeRegistryEntry> m_registeredTypes;

        // Viewport canvas state
        ImVec2 m_pan{ 0.0f, 0.0f };
        uint32_t m_selectedNodeId = 0;
        uint32_t m_draggingPinId = 0;

        // Internal helpers
        void drawDetailPanel(float panelWidth);
        void drawCanvas(const ImVec2& canvasSize);
    };

}
