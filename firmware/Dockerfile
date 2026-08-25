FROM espressif/idf:v5.5.1

ARG IDF_INSTALL_TARGETS=esp32c6

# Install ESP32C6 specific toolchain
RUN cd /opt/esp/idf && \
    ./install.sh ${IDF_INSTALL_TARGETS} && \
    rm -rf .git && \
    echo ". /opt/esp/idf/export.sh" >> ~/.bashrc && \
    # Create cache directory with proper permissions
    mkdir -p /home/esp/.cache/Espressif/ComponentManager && \
    chmod -R 777 /home/esp/.cache
