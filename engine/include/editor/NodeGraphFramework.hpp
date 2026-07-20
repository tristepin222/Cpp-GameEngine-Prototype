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
        std::string name; // e.g. "float", "int", "bool", "Vec3", "Dialogue"
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

        // Optional rendering callback for internal widgets (sliders, inputs)
        std::function<void(Node&)> customWidgetCallback = nullptr;
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
     */
    class NodeGraph {
    public:
        NodeGraph();
        ~NodeGraph();

        // Node / Pin / Link editing API (Safe ID-based interface to prevent reference invalidation)
        uint32_t createNode(const std::string& title, const std::string& typeName, ImVec2 pos = ImVec2(100.0f, 100.0f));
        uint32_t addInputPin(uint32_t nodeId, const std::string& name, const NodePinType& type);
        uint32_t addOutputPin(uint32_t nodeId, const std::string& name, const NodePinType& type);

        void addLink(uint32_t fromPinId, uint32_t toPinId);
        void removeLink(uint32_t linkId);
        void deleteNode(uint32_t nodeId);
        void clear();

        // Getters and Search Helpers
        Node* findNode(uint32_t nodeId);
        NodePin* findPin(uint32_t pinId);
        NodeLink* findLink(uint32_t linkId);
        NodeLink* findLinkConnectingToInput(uint32_t inputPinId);

        const std::vector<Node>& getNodes() const { return m_nodes; }
        const std::vector<NodeLink>& getLinks() const { return m_links; }

        // Serialization
        std::string serialize();
        bool deserialize(const std::string& jsonStr);

        // Rendering API
        void draw(const char* editorId, const ImVec2& canvasSize = ImVec2(0.0f, 0.0f));

        // Registry of node types for right-click creation menu
        struct NodeRegistryEntry {
            std::string category;
            std::string typeName;
            std::string title;
            std::function<void(uint32_t)> populateCallback;
            std::function<void(Node&)> customWidgetCallback;
        };

        void registerNodeType(const std::string& category, const std::string& typeName, const std::string& title, 
                              std::function<void(uint32_t)> populateCallback, 
                              std::function<void(Node&)> customWidgetCallback = nullptr);
        const std::vector<NodeRegistryEntry>& getRegisteredNodeTypes() const { return m_registeredTypes; }

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
        bool m_isPanning = false;
        ImVec2 m_panStartMouse{ 0.0f, 0.0f };
        ImVec2 m_panStartVal{ 0.0f, 0.0f };
    };

}
