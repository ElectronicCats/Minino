#!/bin/bash

PARTITIONS_FILE_OTA="partitions.csv"
PARTITIONS_FILE_NO_OTA="partitions_no_ota.csv"

PROJECT_PATH="."

# Función para mostrar cómo usar el script
function usage() {
    echo "Uso: $0 [--ota | --no-ota]"
    echo "  --ota      Compila el proyecto con soporte para OTA."
    echo "  --no-ota   Compila el proyecto sin soporte para OTA."
    exit 1
}

# Verificación de argumentos
if [ "$#" -ne 1 ]; then
    usage
fi

# Verificación de entorno IDF
if [ -z "$IDF_PATH" ]; then
    echo "Error: El entorno IDF no está configurado. Asegúrate de que el IDF_PATH esté definido y el entorno esté activado."
    exit 1
fi


# Selección del archivo de particiones basado en el argumento
case "$1" in
    --ota)
        echo "Compilando con soporte para OTA..."
        PARTITIONS_FILE=$PARTITIONS_FILE_OTA
        ;;
    --no-ota)
        echo "Compilando sin soporte para OTA..."
        PARTITIONS_FILE=$PARTITIONS_FILE_NO_OTA
        ;;
    *)
        usage
        ;;
esac

# Comprobación si el archivo de particiones existe
if [ ! -f "$PARTITIONS_FILE" ]; then
    echo "Error: No se encuentra el archivo de particiones $PARTITIONS_FILE."
    exit 1
fi
# Ejecutar build
idf.py build 

# Verificar si la compilación fue exitosa
if [ $? -eq 0 ]; then
    echo "Compilación completada con éxito."
else
    echo "Error: Falló la compilación."
    exit 1
fi
