"""
test_interference.py - Tests unitarios para modules/core/interference.py
"""

import pandas as pd
import pytest
from unittest.mock import patch

from modules.core.interference import (
    _classify_signal,
    _format_channels_list,
    _generate_comprehensive_analysis,
    _NON_OVERLAPPING,
)


class TestClassifySignal:

    @pytest.mark.parametrize("rssi,esperado", [
        (-49,  "Excelente"),
        (-50,  "Excelente"),
        (-51,  "Buena"),
        (-59,  "Buena"),
        (-60,  "Buena"),
        (-61,  "Regular"),
        (-69,  "Regular"),
        (-70,  "Regular"),
        (-71,  "Débil"),
        (-79,  "Débil"),
        (-80,  "Débil"),
        (-81,  "Muy débil"),
        (-100, "Muy débil"),
    ])
    def test_clasificacion_por_umbral(self, rssi, esperado):
        assert _classify_signal(rssi) == esperado

    def test_señal_perfecta(self):
        assert _classify_signal(0) == "Excelente"

    def test_señal_muy_lejana(self):
        assert _classify_signal(-120) == "Muy débil"


class TestFormatChannelsList:

    def test_convierte_strings_a_int(self):
        resultado = _format_channels_list(["1", "6", "11"])
        assert resultado == [1, 6, 11]

    def test_convierte_floats_a_int(self):
        resultado = _format_channels_list([1.0, 6.0, 11.0])
        assert resultado == [1, 6, 11]

    def test_lista_vacia(self):
        assert _format_channels_list([]) == []

    def test_tipo_de_retorno_es_lista(self):
        assert isinstance(_format_channels_list([1, 6]), list)

    def test_todos_elementos_son_int(self):
        # int() no acepta "7.0" como string → pasar valores ya convertibles
        resultado = _format_channels_list(["3", 7, 11])
        assert all(isinstance(c, int) for c in resultado)


class TestNonOverlapping:

    def test_canales_no_superpuestos_son_correctos(self):
        assert _NON_OVERLAPPING == [1, 6, 11]

    def test_es_lista(self):
        assert isinstance(_NON_OVERLAPPING, list)


class TestGenerateComprehensiveAnalysis:

    def test_retorna_string(self, df_canales_interferencia):
        df = df_canales_interferencia.copy()
        resultado = _generate_comprehensive_analysis(df)
        assert isinstance(resultado, str)

    def test_contiene_resumen_ejecutivo(self, df_canales_interferencia):
        resultado = _generate_comprehensive_analysis(df_canales_interferencia)
        assert "RESUMEN EJECUTIVO" in resultado

    def test_contiene_total_redes(self, df_canales_interferencia):
        resultado = _generate_comprehensive_analysis(df_canales_interferencia)
        assert "20" in resultado  # 20 redes en el fixture

    def test_contiene_canales_no_superpuestos(self, df_canales_interferencia):
        resultado = _generate_comprehensive_analysis(df_canales_interferencia)
        assert "Canal 1" in resultado
        assert "Canal 6" in resultado
        assert "Canal 11" in resultado

    def test_contiene_recomendaciones(self, df_canales_interferencia):
        resultado = _generate_comprehensive_analysis(df_canales_interferencia)
        assert "RECOMENDACIONES" in resultado

    def test_calcula_porcentaje_señal_debil(self, df_canales_interferencia):
        df = df_canales_interferencia.copy()
        resultado = _generate_comprehensive_analysis(df)
        # 5 redes con RSSI -85 son <= -80 → 25%
        assert "25.0%" in resultado

    def test_con_todas_señales_fuertes(self):
        df = pd.DataFrame({
            "SSID":    [f"Red{i}" for i in range(5)],
            "RSSI":    [-45.0] * 5,
            "Channel": [1, 1, 6, 6, 11],
        })
        resultado = _generate_comprehensive_analysis(df)
        assert "0 redes (0.0% del total)" in resultado
