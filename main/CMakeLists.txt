idf_component_register(SRCS "main.c" "wifi_module.c" "mqtt_module.c" 
                    INCLUDE_DIRS ".")
target_add_binary_data(${COMPONENT_TARGET} "insator_server.crt.pem" TEXT)