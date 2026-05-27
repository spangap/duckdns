file(GLOB DUCKDNS_SRCS "src/*.cpp")

idf_component_register(
    SRCS ${DUCKDNS_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES
        spangap-core
        spangap-net
        spangap-lcd
        upnp
        esp_event
        esp_netif
        lwip
        esp_http_client
)
