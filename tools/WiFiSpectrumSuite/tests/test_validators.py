"""
test_validators.py - Tests unitarios para modules/utils/validators.py
"""

import pytest
from modules.utils.validators import looks_like_date, repair_date_field


class TestLooksLikeDate:

    # --- Casos que SÍ deben reconocerse como fecha ---

    @pytest.mark.parametrize("valor", [
        "2024-01-15 10:30:00",
        "2024-01-15",
        "01/15/2024 10:30:00",
        "01/15/2024",
        "15-01-2024 10:30:00",
        "2024/01/15 10:30:00",
        "2023-12-25",
        "2025-06-01 00:00:00",
        "10 Jan 2024",
        "3:00 PM",
        "12:00 am",
    ])
    def test_reconoce_fechas_validas(self, valor):
        assert looks_like_date(valor) is True

    # --- Casos que NO son fechas ---

    @pytest.mark.parametrize("valor", [
        "WPA2",
        "WEP",
        "OPEN",
        "RedCasa",
        "192.168.1.1",
        "-65",
        "6",
    ])
    def test_rechaza_no_fechas(self, valor):
        assert looks_like_date(valor) is False

    # --- Casos borde ---

    def test_cadena_vacia(self):
        assert looks_like_date("") is False

    def test_solo_espacios(self):
        assert looks_like_date("   ") is False

    def test_none(self):
        assert looks_like_date(None) is False

    def test_numero_entero(self):
        # Un número sin año reconocible no es fecha
        assert looks_like_date(42) is False

    def test_año_reconocible(self):
        # Contiene "2024" como substring → True por keywords
        assert looks_like_date("Evento 2024") is True


class TestRepairDateField:

    # --- Valores que ya son fechas válidas → normaliza al formato canónico ---

    def test_formato_iso_sin_cambio(self):
        resultado = repair_date_field("2024-01-15 10:30:00")
        assert resultado == "2024-01-15 10:30:00"

    def test_formato_dia_mes_año(self):
        resultado = repair_date_field("15/01/2024 10:30:00")
        assert resultado == "2024-01-15 10:30:00"

    def test_formato_mes_dia_año(self):
        resultado = repair_date_field("01/15/2024 10:30:00")
        assert resultado == "2024-01-15 10:30:00"

    def test_formato_año_barra(self):
        resultado = repair_date_field("2024/01/15 10:30:00")
        assert resultado == "2024-01-15 10:30:00"

    def test_formato_dia_guion_mes_año(self):
        resultado = repair_date_field("15-01-2024 10:30:00")
        assert resultado == "2024-01-15 10:30:00"

    # --- Valores que son protocolos WiFi → reemplaza con fecha actual ---

    @pytest.mark.parametrize("protocolo", ["WPA2", "WPA", "WEP", "OPN", "OPEN", "UNKNOWN", "N/A", "NULL"])
    def test_protocolo_wifi_devuelve_fecha(self, protocolo):
        resultado = repair_date_field(protocolo)
        # Debe ser parseable como fecha ISO
        from datetime import datetime
        parsed = datetime.strptime(resultado, "%Y-%m-%d %H:%M:%S")
        assert parsed is not None

    # --- Casos borde ---

    def test_cadena_vacia_devuelve_mismo_valor(self):
        assert repair_date_field("") == ""

    def test_none_devuelve_none(self):
        assert repair_date_field(None) is None

    def test_valor_irreconocible_devuelve_original(self):
        valor = "texto_sin_formato"
        assert repair_date_field(valor) == valor

    def test_protocolo_en_minusculas_se_detecta(self):
        # El check usa value_str.upper(), así que "wpa2" → "WPA2" que SÍ está en non_date_values
        from datetime import datetime
        resultado = repair_date_field("wpa2")
        parsed = datetime.strptime(resultado, "%Y-%m-%d %H:%M:%S")
        assert parsed is not None
