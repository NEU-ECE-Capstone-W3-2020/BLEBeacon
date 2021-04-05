#ifndef CONNECTION_WRAPPER_H
#define CONNECTION_WRAPPER_H

//#include <Arduino>

class ConnectionWrapper {
protected:
    String _name;
    uint16_t _conn_handle;

public:
    ConnectionWrapper();
    ~ConnectionWrapper() = default;

    String getName() const;
    void setName(String name);
    uint16_t getConnHandle() const;
    void setConnHandle(uint16_t handle);
    bool isInvalid() const;
    void invalidate();
};

#endif
