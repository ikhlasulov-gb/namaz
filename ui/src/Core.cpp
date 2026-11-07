#include "Core.h"

Core::Core() : m_handle(namaz_core_new()) {}

Core::~Core() {
    if (m_handle) {
        namaz_core_free(m_handle);
    }
}

QString takeCoreString(char *value) {
    if (!value) {
        return {};
    }
    const QString result = QString::fromUtf8(value);
    namaz_core_free_string(value);
    return result;
}
