"""
test_wardriving.py - Tests unitarios para modules/core/wardriving.py
"""

import pandas as pd
import pytest
from unittest.mock import MagicMock, call, patch

from modules.core.wardriving import WardrivingAnalyzer, _COLUMN_ALIASES


class TestColumnAliases:

    def test_contiene_columnas_canonicas(self):
        esperadas = {"SSID", "MAC", "FirstSeen", "Channel", "Frequency", "RSSI",
                     "CurrentLatitude", "CurrentLongitude", "AuthMode"}
        assert set(_COLUMN_ALIASES.keys()) == esperadas

    def test_ssid_tiene_alias(self):
        assert len(_COLUMN_ALIASES["SSID"]) >= 1
        assert "SSID" in _COLUMN_ALIASES["SSID"]

    def test_rssi_tiene_alias_signal(self):
        assert "Signal" in _COLUMN_ALIASES["RSSI"]

    def test_latitud_tiene_alias_latitude(self):
        assert "Latitude" in _COLUMN_ALIASES["CurrentLatitude"]

    def test_longitud_tiene_alias_longitude(self):
        assert "Longitude" in _COLUMN_ALIASES["CurrentLongitude"]


class TestWardrivingAnalyzerMapearColumnas:

    def _analyzer_con_df(self, df, csv_path="fake.csv"):
        analyzer = WardrivingAnalyzer(csv_path)
        analyzer.df = df.copy()
        return analyzer

    def test_mapea_signal_a_rssi(self):
        df = pd.DataFrame({"SSID": ["Red1"], "Signal": [-50.0], "Channel": [1]})
        analyzer = self._analyzer_con_df(df)
        analyzer._mapear_columnas()
        assert "RSSI" in analyzer.df.columns

    def test_mapea_latitude_a_currentlatitude(self):
        df = pd.DataFrame({
            "SSID": ["Red1"], "RSSI": [-50.0], "Channel": [1],
            "Latitude": [19.43], "Longitude": [-99.13],
        })
        analyzer = self._analyzer_con_df(df)
        analyzer._mapear_columnas()
        assert "CurrentLatitude" in analyzer.df.columns

    def test_no_sobreescribe_columna_existente(self):
        df = pd.DataFrame({
            "SSID": ["Red1"], "RSSI": [-50.0], "Signal": [-99.0], "Channel": [1],
        })
        analyzer = self._analyzer_con_df(df)
        analyzer._mapear_columnas()
        # RSSI ya existe → no debe sobreescribirse con Signal
        assert analyzer.df["RSSI"].iloc[0] == -50.0

    def test_sin_aliases_disponibles_no_falla(self):
        df = pd.DataFrame({"SSID": ["Red1"], "RSSI": [-50.0], "Channel": [1]})
        analyzer = self._analyzer_con_df(df)
        analyzer._mapear_columnas()   # No debe lanzar excepción


class TestWardrivingAnalyzerConvertirTipos:

    def _analyzer_con_df(self, df):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = df.copy()
        return analyzer

    def test_convierte_channel_a_int(self):
        df = pd.DataFrame({
            "SSID": ["Red1"], "RSSI": [-50.0],
            "Channel": ["6"], "Frequency": ["2437"], "FirstSeen": ["2024-01-15 10:00:00"],
        })
        analyzer = self._analyzer_con_df(df)
        analyzer._convertir_tipos()
        assert analyzer.df["Channel"].dtype in (int, "int64", "int32")

    def test_convierte_rssi_a_numerico(self):
        # pd.to_numeric puede inferir int o float según el valor
        df = pd.DataFrame({
            "SSID": ["Red1"], "RSSI": ["-50"],
            "Channel": [6], "Frequency": [2437], "FirstSeen": ["2024-01-15 10:00:00"],
        })
        analyzer = self._analyzer_con_df(df)
        analyzer._convertir_tipos()
        assert pd.api.types.is_numeric_dtype(analyzer.df["RSSI"])

    def test_crea_columna_timestamp(self):
        df = pd.DataFrame({
            "SSID": ["Red1"], "RSSI": [-50.0],
            "Channel": [6], "Frequency": [2437],
            "FirstSeen": ["2024-01-15 10:00:00"],
        })
        analyzer = self._analyzer_con_df(df)
        analyzer._convertir_tipos()
        assert "Timestamp" in analyzer.df.columns

    def test_elimina_filas_sin_coordenadas(self):
        df = pd.DataFrame({
            "SSID": ["Red1", "Red2"],
            "RSSI": [-50.0, -60.0],
            "Channel": [1, 6],
            "Frequency": [2412, 2437],
            "FirstSeen": ["2024-01-15 10:00:00", "2024-01-15 10:01:00"],
            "CurrentLatitude":  [19.43, None],
            "CurrentLongitude": [-99.13, None],
        })
        analyzer = self._analyzer_con_df(df)
        analyzer._convertir_tipos()
        assert len(analyzer.df) == 1


class TestWardrivingAnalyzerAnalizarGeneral:

    def test_retorna_dict_con_claves_esperadas(self, df_wifi_valido):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = df_wifi_valido.copy()
        resultado = analyzer.analizar_general()

        assert "total_registros" in resultado
        assert "redes_unicas" in resultado
        assert "top_redes" in resultado
        assert "metricas_rssi" in resultado

    def test_total_registros_correcto(self, df_wifi_valido):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = df_wifi_valido.copy()
        resultado = analyzer.analizar_general()
        assert resultado["total_registros"] == 5

    def test_redes_unicas_correctas(self, df_wifi_valido):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = df_wifi_valido.copy()
        resultado = analyzer.analizar_general()
        assert resultado["redes_unicas"] == 5

    def test_metricas_rssi_correctas(self, df_wifi_valido):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = df_wifi_valido.copy()
        resultado = analyzer.analizar_general()
        rssi = resultado["metricas_rssi"]

        assert rssi["minimo"] == -91.0
        assert rssi["maximo"] == -45.0
        assert abs(rssi["promedio"] - (-71.0)) < 0.1

    def test_df_nulo_devuelve_dict_vacio(self):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = None
        assert analyzer.analizar_general() == {}

    def test_df_vacio_devuelve_dict_vacio(self):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = pd.DataFrame()
        assert analyzer.analizar_general() == {}

    def test_top_redes_es_dict(self, df_wifi_valido):
        analyzer = WardrivingAnalyzer("fake.csv")
        analyzer.df = df_wifi_valido.copy()
        resultado = analyzer.analizar_general()
        assert isinstance(resultado["top_redes"], dict)


class TestWardrivingAnalyzerCargarDatos:

    def test_retorna_false_si_archivo_no_existe(self):
        analyzer = WardrivingAnalyzer("/no/existe/archivo.csv")
        assert analyzer.cargar_datos() is False

    def test_retorna_true_con_csv_valido(self, csv_valido):
        analyzer = WardrivingAnalyzer(csv_valido)
        assert analyzer.cargar_datos() is True

    def test_df_cargado_no_es_none(self, csv_valido):
        analyzer = WardrivingAnalyzer(csv_valido)
        analyzer.cargar_datos()
        assert analyzer.df is not None

    def test_df_tiene_datos(self, csv_valido):
        analyzer = WardrivingAnalyzer(csv_valido)
        analyzer.cargar_datos()
        assert len(analyzer.df) > 0


class TestAnalizarSpoofing:

    def _analyzer_con_df(self, df, output_dir="."):
        analyzer = WardrivingAnalyzer("fake.csv", output_dir=output_dir)
        analyzer.df = df.copy()
        return analyzer

    def _calls_str(self, mock_console):
        return " ".join(str(c) for c in mock_console.print.call_args_list)

    def test_sin_spoofing_no_alerta_evil_twin(self, tmp_path):
        """SSIDs con un único MAC no generan alerta de Evil Twin."""
        df = pd.DataFrame({
            "SSID":     ["RedCasa", "Oficina", "CafeNet"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02", "AA:BB:CC:DD:EE:03"],
            "AuthMode": ["WPA2", "WPA2", "WPA2"],
        })
        analyzer = self._analyzer_con_df(df, output_dir=str(tmp_path))
        with patch("modules.core.wardriving.console") as mock_console:
            analyzer._analizar_spoofing()
            assert "¡ALERTA EVIL TWIN!" not in self._calls_str(mock_console)

    def test_evil_twin_open_vs_wpa2(self, tmp_path):
        """SSID con OPEN y WPA2 en MACs distintos debe disparar alerta de Evil Twin."""
        df = pd.DataFrame({
            "SSID":     ["RedPublica", "RedPublica"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "FF:EE:DD:CC:BB:AA"],
            "AuthMode": ["OPEN", "WPA2"],
        })
        analyzer = self._analyzer_con_df(df, output_dir=str(tmp_path))
        with patch("modules.core.wardriving.console") as mock_console:
            analyzer._analizar_spoofing()
            assert "¡ALERTA EVIL TWIN!" in self._calls_str(mock_console)

    def test_evil_twin_wep_vs_open(self, tmp_path):
        """WEP + OPEN en mismo SSID también debe detectar Evil Twin."""
        df = pd.DataFrame({
            "SSID":     ["RedLegacy", "RedLegacy"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "FF:EE:DD:CC:BB:AA"],
            "AuthMode": ["WEP", "OPEN"],
        })
        analyzer = self._analyzer_con_df(df, output_dir=str(tmp_path))
        with patch("modules.core.wardriving.console") as mock_console:
            analyzer._analizar_spoofing()
            assert "¡ALERTA EVIL TWIN!" in self._calls_str(mock_console)

    def test_multi_mac_misma_auth_no_es_evil_twin(self, tmp_path):
        """Múltiples MACs con la misma seguridad (mesh/corporativo) no es Evil Twin."""
        df = pd.DataFrame({
            "SSID":     ["CorpWiFi"] * 3,
            "MAC":      ["AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02", "AA:BB:CC:DD:EE:03"],
            "AuthMode": ["WPA2"] * 3,
        })
        analyzer = self._analyzer_con_df(df, output_dir=str(tmp_path))
        with patch("modules.core.wardriving.console") as mock_console:
            analyzer._analizar_spoofing()
            assert "¡ALERTA EVIL TWIN!" not in self._calls_str(mock_console)

    def test_nota_multi_mac_aparece(self, tmp_path):
        """Si hay SSIDs con múltiples MACs, debe mostrarse la nota informativa."""
        df = pd.DataFrame({
            "SSID":     ["CorpWiFi", "CorpWiFi"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02"],
            "AuthMode": ["WPA2", "WPA2"],
        })
        analyzer = self._analyzer_con_df(df, output_dir=str(tmp_path))
        with patch("modules.core.wardriving.console") as mock_console:
            analyzer._analizar_spoofing()
            assert "múltiples MACs" in self._calls_str(mock_console)

    def test_df_solo_nulos_no_falla(self, tmp_path):
        """DataFrame con solo NaN en columnas clave no debe lanzar excepción."""
        df = pd.DataFrame({
            "SSID":     [None, None],
            "MAC":      [None, None],
            "AuthMode": [None, None],
        })
        analyzer = self._analyzer_con_df(df, output_dir=str(tmp_path))
        analyzer._analizar_spoofing()

    def test_mensaje_sin_indicadores_cuando_sin_evil_twin(self, tmp_path):
        """Cuando no hay Evil Twins se imprime el mensaje de ausencia de indicadores."""
        df = pd.DataFrame({
            "SSID":     ["RedCasa"],
            "MAC":      ["AA:BB:CC:DD:EE:01"],
            "AuthMode": ["WPA2"],
        })
        analyzer = self._analyzer_con_df(df, output_dir=str(tmp_path))
        with patch("modules.core.wardriving.console") as mock_console:
            analyzer._analizar_spoofing()
            assert "indicadores" in self._calls_str(mock_console).lower()

    # ------------------------------------------------------------------
    # Tests de generación de reporte CSV
    # ------------------------------------------------------------------

    def test_sin_multi_mac_no_genera_csv(self, tmp_path):
        """Con SSIDs de MAC única no se genera ningún archivo CSV."""
        df = pd.DataFrame({
            "SSID":     ["RedCasa", "Oficina"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02"],
            "AuthMode": ["WPA2", "WPA2"],
        })
        analyzer = self._analyzer_con_df(df, output_dir=str(tmp_path))
        analyzer._analizar_spoofing()
        assert not any(tmp_path.iterdir())

    def test_evil_twin_genera_csv(self, tmp_path):
        """Cuando se detecta un Evil Twin, debe generarse el CSV en output_dir."""
        df = pd.DataFrame({
            "SSID":     ["RedPublica", "RedPublica"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "FF:EE:DD:CC:BB:AA"],
            "AuthMode": ["OPEN", "WPA2"],
        })
        analyzer = self._analyzer_con_df(df, output_dir=str(tmp_path))
        analyzer._analizar_spoofing()
        archivos = list(tmp_path.iterdir())
        assert len(archivos) == 1
        assert archivos[0].suffix == ".csv"

    def test_csv_contiene_alerta_evil_twin(self, tmp_path):
        """El CSV generado para un Evil Twin debe contener la columna Alerta con 'Evil Twin'."""
        df = pd.DataFrame({
            "SSID":     ["RedPublica", "RedPublica"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "FF:EE:DD:CC:BB:AA"],
            "AuthMode": ["OPEN", "WPA2"],
        })
        analyzer = self._analyzer_con_df(df, output_dir=str(tmp_path))
        analyzer._analizar_spoofing()
        csv_file = next(tmp_path.iterdir())
        contenido = csv_file.read_text(encoding="utf-8")
        assert "Evil Twin" in contenido
        assert "RedPublica" in contenido

    def test_csv_contiene_multi_mac_sin_evil_twin(self, tmp_path):
        """El CSV generado para un SSID mesh/corporativo debe reflejar su tipo de alerta."""
        df = pd.DataFrame({
            "SSID":     ["CorpWiFi", "CorpWiFi"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02"],
            "AuthMode": ["WPA2", "WPA2"],
        })
        analyzer = self._analyzer_con_df(df, output_dir=str(tmp_path))
        analyzer._analizar_spoofing()
        csv_file = next(tmp_path.iterdir())
        contenido = csv_file.read_text(encoding="utf-8")
        assert "Multi-MAC" in contenido
        assert "CorpWiFi" in contenido

    def test_csv_tiene_cabeceras_correctas(self, tmp_path):
        """El CSV debe contener las cuatro columnas definidas: SSID, MAC, AuthMode, Alerta."""
        df = pd.DataFrame({
            "SSID":     ["RedPublica", "RedPublica"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "FF:EE:DD:CC:BB:AA"],
            "AuthMode": ["OPEN", "WPA2"],
        })
        analyzer = self._analyzer_con_df(df, output_dir=str(tmp_path))
        analyzer._analizar_spoofing()
        csv_file = next(tmp_path.iterdir())
        primera_linea = csv_file.read_text(encoding="utf-8").splitlines()[0]
        assert primera_linea == "SSID,MAC,AuthMode,Alerta"

    def test_csv_nombre_incluye_nombre_base(self, tmp_path):
        """El nombre del archivo CSV debe incluir el nombre base del CSV de origen."""
        df = pd.DataFrame({
            "SSID":     ["RedPublica", "RedPublica"],
            "MAC":      ["AA:BB:CC:DD:EE:01", "FF:EE:DD:CC:BB:AA"],
            "AuthMode": ["OPEN", "WPA2"],
        })
        analyzer = WardrivingAnalyzer("mis_datos.csv", output_dir=str(tmp_path))
        analyzer.df = df.copy()
        analyzer._analizar_spoofing()
        archivos = [f.name for f in tmp_path.iterdir()]
        assert any("mis_datos" in nombre for nombre in archivos)
