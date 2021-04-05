#include <bluefruit.h>

#include "connection_wrapper.h"

ConnectionWrapper::ConnectionWrapper() : _name(), _conn_handle(BLE_CONN_HANDLE_INVALID) {}

String ConnectionWrapper::getName() const {
    return this->_name;
}

void ConnectionWrapper::setName(String name) {
    this->_name = name;
}

uint16_t ConnectionWrapper::getConnHandle() const {
    return this->_conn_handle;
}

void ConnectionWrapper::setConnHandle(uint16_t handle) {
    this->_conn_handle = handle;
}

bool ConnectionWrapper::isInvalid() const {
    return this->_conn_handle == BLE_CONN_HANDLE_INVALID;
}

void ConnectionWrapper::invalidate() {
    this->_conn_handle = BLE_CONN_HANDLE_INVALID;
    this->_name = String();
}
