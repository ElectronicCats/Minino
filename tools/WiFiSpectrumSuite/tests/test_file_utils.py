"""
test_file_utils.py - Tests unitarios para modules/utils/file_utils.py
"""

import numpy as np
import pandas as pd
import pytest
from unittest.mock import patch

from modules.utils.file_utils import clean_and_validate_data, _manual_csv_loader


class TestCleanAndValidateData:

    def test_elimina_filas_con_rssi_nulo(self, df_con_nulos):
        resultado = clean_and_validate_data(df_con_nulos.copy())
        assert resultado["RSSI"].isna().sum() == 0

    def test_elimina_filas_con_channel_nulo(self, df_con_nulos):
        resultado = clean_and_validate_data(df_con_nulos.copy())
        assert resultado["Channel"].isna().sum() == 0

    def test_conserva_filas_completas(self, df_con_nulos):
        resultado = clean_and_validate_data(df_con_nulos.copy())
        # df_con_nulos tiene 4 filas; Red2 (RSSI nulo) y Red3 (Channel nulo) se eliminan
        assert len(resultado) == 2

    def test_convierte_rssi_a_numerico(self):
        df = pd.DataFrame({
            "SSID":    ["Red1", "Red2"],
            "RSSI":    ["-50", "-70"],   # strings
            "Channel": [1, 6],
        })
        resultado = clean_and_validate_data(df)
        assert pd.api.types.is_float_dtype(resultado["RSSI"]) or pd.api.types.is_integer_dtype(resultado["RSSI"])

    def test_convierte_channel_a_numerico(self):
        df = pd.DataFrame({
            "SSID":    ["Red1", "Red2"],
            "RSSI":    [-50.0, -70.0],
            "Channel": ["1", "6"],   # strings
        })
        resultado = clean_and_validate_data(df)
        assert pd.api.types.is_numeric_dtype(resultado["Channel"])

    def test_descarta_rssi_no_numerico(self):
        df = pd.DataFrame({
            "SSID":    ["Red1", "Red2", "Red3"],
            "RSSI":    [-50.0, "no_valido", -70.0],
            "Channel": [1, 6, 11],
        })
        resultado = clean_and_validate_data(df)
        assert len(resultado) == 2

    def test_df_vacio_devuelve_vacio(self):
        df = pd.DataFrame({"SSID": [], "RSSI": [], "Channel": []})
        resultado = clean_and_validate_data(df)
        assert resultado.empty

    def test_sin_columna_rssi_lanza_key_error(self):
        # dropna(subset=["RSSI", "Channel"]) falla si RSSI no existe → bug conocido
        df = pd.DataFrame({"SSID": ["Red1"], "Channel": [1]})
        with pytest.raises(KeyError):
            clean_and_validate_data(df)

    def test_conserva_columnas_extras(self, df_wifi_valido):
        resultado = clean_and_validate_data(df_wifi_valido.copy())
        assert "AuthMode" in resultado.columns
        assert "CurrentLatitude" in resultado.columns


class TestManualCsvLoader:

    def test_carga_csv_simple(self, csv_valido):
        df = _manual_csv_loader(csv_valido)
        assert df is not None
        assert len(df) == 3
        assert "SSID" in df.columns

    def test_error_en_archivo_corto(self, csv_corto):
        with pytest.raises(ValueError, match="demasiado corto"):
            _manual_csv_loader(csv_corto)

    def test_completa_columnas_faltantes_con_nan(self, tmp_path):
        # Línea con menos campos que el encabezado
        contenido = (
            "WigleWifi-1.4,appRelease=2.73\n"
            "MAC,SSID,AuthMode,Channel\n"
            "AA:BB,Red1,WPA2\n"       # falta Channel
            "AA:CC,Red2,WPA2,6\n"
        )
        archivo = tmp_path / "incompleto.csv"
        archivo.write_text(contenido, encoding="utf-8")

        df = _manual_csv_loader(str(archivo))
        assert len(df) == 2
        # La primera fila debe tener NaN en Channel
        assert pd.isna(df.iloc[0]["Channel"]) or df.iloc[0]["Channel"] != df.iloc[0]["Channel"]

    def test_trunca_columnas_extra(self, tmp_path):
        # Línea con más campos que el encabezado
        contenido = (
            "WigleWifi-1.4,appRelease=2.73\n"
            "MAC,SSID,Channel\n"
            "AA:BB,Red1,1,campo_extra,otro_extra\n"
            "AA:CC,Red2,6\n"
        )
        archivo = tmp_path / "extra.csv"
        archivo.write_text(contenido, encoding="utf-8")

        df = _manual_csv_loader(str(archivo))
        assert df.shape[1] == 3   # solo 3 columnas del encabezado
