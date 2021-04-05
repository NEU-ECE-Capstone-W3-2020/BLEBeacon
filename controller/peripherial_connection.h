#ifndef PERIPHERIAL_CONNECTION_H
#define PERIPHERIAL_CONNECTION_H

#include <clients/BLEClientUart.h>

#include "connection_wrapper.h"

class PeripheralConnection : public ConnectionWrapper {
public: 
    BLEClientUart bleuart;
};

#endif
