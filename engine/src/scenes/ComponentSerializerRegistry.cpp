#include "scenes/ComponentSerializerRegistry.hpp"

ComponentSerializerRegistry& ComponentSerializerRegistry::getInstance() {
    static ComponentSerializerRegistry instance;
    return instance;
}
