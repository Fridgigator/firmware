idf_component_register(SRCS "ScanResults.cpp"
        "GetSensorData.cpp"
        "getTime.cpp"
        "main.cpp"
        "exceptions/ConnectionException.cpp"
        "exceptions/DecodeException.cpp"
        "exceptions/InterruptedException.cpp"
        "exceptions/StateException.cpp"
        "exceptions/WrongPacketException.cpp"
        "lib/websocket/websocket.cpp"
        "lib/ArduinoSupport/ArduinoSupport.cpp"
        "lib/ArduinoSupport/Preferences.cpp"
        "generated/firmware_backend.pb.c"
        "generated/packet.pb.c"
        INCLUDE_DIRS "."
        REQUIRES
        nanopb
        esp-nimble-cpp
        driver
        )

	# target_compile_options(${COMPONENT_LIB} PRIVATE "-fsanitize=undefined" "-fno-sanitize=shift-base" "-fsanitize=float-cast-overflow")
