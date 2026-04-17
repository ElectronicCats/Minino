"""
test_csv_debugger.py - Tests unitarios para modules/core/csv_debugger.py
"""

import pytest
from unittest.mock import patch

from modules.core.csv_debugger import (
    _find_date_columns,
    analyze_date_problems,
    repair_date_issues,
)


class TestFindDateColumns:

    def test_detecta_columna_time(self):
        headers = ["MAC", "SSID", "FirstTime", "Channel"]
        resultado = _find_date_columns(headers)
        nombres = [nombre for _, nombre in resultado]
        assert "FirstTime" in nombres

    def test_detecta_columna_date(self):
        headers = ["MAC", "SSID", "ScanDate", "RSSI"]
        resultado = _find_date_columns(headers)
        nombres = [nombre for _, nombre in resultado]
        assert "ScanDate" in nombres

    def test_detecta_firstseen_y_lastseen(self):
        headers = ["MAC", "SSID", "FirstSeen", "LastSeen", "Channel"]
        resultado = _find_date_columns(headers)
        nombres = [nombre for _, nombre in resultado]
        assert "FirstSeen" in nombres
        assert "LastSeen" in nombres

    def test_no_detecta_columnas_normales(self):
        headers = ["MAC", "SSID", "Channel", "RSSI", "AuthMode"]
        resultado = _find_date_columns(headers)
        assert resultado == []

    def test_detecta_por_keyword_first(self):
        headers = ["FirstSeen", "LastSeen", "RSSI"]
        resultado = _find_date_columns(headers)
        assert len(resultado) == 2

    def test_devuelve_indices_correctos(self):
        headers = ["MAC", "FirstSeen", "SSID", "LastSeen"]
        resultado = _find_date_columns(headers)
        indices = {nombre: idx for idx, nombre in resultado}
        assert indices["FirstSeen"] == 1
        assert indices["LastSeen"] == 3

    def test_case_insensitive(self):
        # "FIRSTSEEN" contiene "FIRST" → debe detectarse
        headers = ["MAC", "FIRSTSEEN", "SSID"]
        resultado = _find_date_columns(headers)
        assert len(resultado) == 1

    def test_lista_vacia(self):
        assert _find_date_columns([]) == []


class TestAnalyzeDateProblems:

    def test_retorna_headers_en_csv_valido(self, csv_valido):
        headers, _, _ = analyze_date_problems(csv_valido)
        assert headers is not None
        assert "SSID" in headers
        assert "FirstSeen" in headers

    def test_detecta_columnas_de_fecha(self, csv_valido):
        _, _, date_columns = analyze_date_problems(csv_valido)
        nombres = [nombre for _, nombre in date_columns]
        assert "FirstSeen" in nombres

    def test_csv_sin_problemas_de_fecha(self, csv_valido):
        _, problematic, _ = analyze_date_problems(csv_valido)
        assert len(problematic) == 0

    def test_detecta_valores_rotos(self, csv_con_fechas_rotas):
        _, problematic, _ = analyze_date_problems(csv_con_fechas_rotas)
        # "WPA2" en FirstSeen no parece fecha
        assert len(problematic) > 0

    def test_retorna_none_en_archivo_inexistente(self):
        headers, problematic, date_cols = analyze_date_problems("/no/existe.csv")
        assert headers is None
        assert problematic == []
        assert date_cols == []

    def test_retorna_none_en_archivo_muy_corto(self, csv_corto):
        headers, _, _ = analyze_date_problems(csv_corto)
        assert headers is None


class TestRepairDateIssues:

    def test_genera_archivo_reparado(self, csv_con_fechas_rotas, tmp_path):
        output = str(tmp_path / "reparado.csv")
        resultado = repair_date_issues(csv_con_fechas_rotas, output_file=output)
        assert resultado == output

    def test_archivo_reparado_existe(self, csv_con_fechas_rotas, tmp_path):
        import os
        output = str(tmp_path / "reparado.csv")
        repair_date_issues(csv_con_fechas_rotas, output_file=output)
        assert os.path.exists(output)

    def test_nombre_output_por_defecto(self, csv_con_fechas_rotas, tmp_path):
        import os
        resultado = repair_date_issues(csv_con_fechas_rotas, output_dir=str(tmp_path))
        assert resultado is not None
        assert resultado.endswith("_fixed.csv")
        assert os.path.exists(resultado)

    def test_retorna_none_en_archivo_inexistente(self):
        resultado = repair_date_issues("/no/existe.csv")
        assert resultado is None

    def test_lineas_preservadas(self, csv_valido, tmp_path):
        output = str(tmp_path / "reparado.csv")
        repair_date_issues(csv_valido, output_file=output)
        with open(output, encoding="utf-8") as f:
            lineas = f.readlines()
        # El CSV tiene 2 líneas de cabecera + 3 de datos = 5 líneas
        assert len(lineas) == 5
