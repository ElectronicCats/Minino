"""
test_completion.py - Unit tests for the shell completion command (modules/cli.py)
"""

import os
import sys
from pathlib import Path
from unittest.mock import MagicMock, patch
import pytest
from click.testing import CliRunner

from modules.cli import cli, completion


# ---------------------------------------------------------------------------
# Helpers / fake data
# ---------------------------------------------------------------------------

FAKE_PYTHON = "/fake/venv/bin/python"
FAKE_SCRIPT = "/fake/path/wifi_spectrum.py"

ZSH_CLICK_SCRIPT = "\n".join([
    "#compdef wifi-spectrum",
    "",
    "_wifi_spectrum_completion() {",
    "    (( ! $+commands[wifi-spectrum] )) && return 1",
    '    response=("${(@f)$(env COMP_WORDS="${words[*]}" COMP_CWORD=$((CURRENT-1))'
    ' _WIFI_SPECTRUM_COMPLETE=zsh_complete wifi-spectrum)}")',
    "}",
    "",
    "if [[ $zsh_eval_context[-1] == loadautofunc ]]; then",
    '    _wifi_spectrum_completion "$@"',
    "else",
    "    compdef _wifi_spectrum_completion wifi-spectrum",
    "fi",
    "",
])

BASH_CLICK_SCRIPT = "\n".join([
    "_wifi_spectrum_completion() {",
    '    COMPREPLY=( $( env COMP_WORDS="${COMP_WORDS[*]}" COMP_CWORD=$COMP_CWORD'
    " _WIFI_SPECTRUM_COMPLETE=bash_complete wifi-spectrum ) )",
    "    return 0",
    "}",
    "complete -F _wifi_spectrum_completion wifi-spectrum",
    "",
])

FISH_CLICK_SCRIPT = "\n".join([
    "function _wifi_spectrum_completion",
    "    env _WIFI_SPECTRUM_COMPLETE=fish_complete wifi-spectrum",
    "end",
    "",
])


def _sp_result(stdout, returncode=0):
    m = MagicMock()
    m.stdout = stdout
    m.returncode = returncode
    return m


def _run_install(shell, tmp_path, script):
    """Invoke 'completion install' with standard mocks and return (result, written_script)."""
    runner = CliRunner()
    with (
        patch("modules.cli.platform.system", return_value="Linux"),
        patch("subprocess.run", return_value=_sp_result(script)),
        patch("pathlib.Path.home", return_value=tmp_path),
        patch.object(sys, "executable", FAKE_PYTHON),
        patch.object(sys, "argv", [FAKE_SCRIPT]),
    ):
        result = runner.invoke(completion, ["install", "--shell", shell])

    targets = {
        "zsh":  tmp_path / ".zfunc" / "_wifi-spectrum",
        "bash": tmp_path / ".local" / "share" / "bash-completion" / "completions" / "wifi-spectrum",
        "fish": tmp_path / ".config" / "fish" / "completions" / "wifi-spectrum.fish",
    }
    target = targets[shell]
    written = target.read_text() if target.exists() else ""
    return result, written


# ---------------------------------------------------------------------------
# Test classes
# ---------------------------------------------------------------------------

class TestCompletionCommandRegistration:

    def test_install_subcommand_exists(self):
        assert "install" in completion.commands

    def test_completion_added_to_cli_on_linux(self):
        with patch("modules.cli.platform.system", return_value="Linux"):
            import modules.cli as m
            m.cli.add_command(m.completion)
            assert "completion" in m.cli.commands


class TestPlatformGuard:

    def test_windows_exits_with_error(self):
        with patch("modules.cli.platform.system", return_value="Windows"):
            result = CliRunner().invoke(completion, ["install", "--shell", "zsh"])
        assert result.exit_code != 0


class TestShellAutoDetection:

    def _auto_detect(self, shell_path, tmp_path, script):
        runner = CliRunner()
        with (
            patch("modules.cli.platform.system", return_value="Linux"),
            patch("subprocess.run", return_value=_sp_result(script)),
            patch("pathlib.Path.home", return_value=tmp_path),
            patch.object(sys, "executable", FAKE_PYTHON),
            patch.object(sys, "argv", [FAKE_SCRIPT]),
            patch.dict(os.environ, {"SHELL": shell_path}),
        ):
            return runner.invoke(completion, ["install"])

    def test_detects_zsh(self, tmp_path):
        result = self._auto_detect("/usr/bin/zsh", tmp_path, ZSH_CLICK_SCRIPT)
        assert result.exit_code == 0

    def test_detects_bash(self, tmp_path):
        result = self._auto_detect("/bin/bash", tmp_path, BASH_CLICK_SCRIPT)
        assert result.exit_code == 0

    def test_detects_fish(self, tmp_path):
        result = self._auto_detect("/usr/bin/fish", tmp_path, FISH_CLICK_SCRIPT)
        assert result.exit_code == 0

    def test_unknown_shell_exits_with_error(self):
        with (
            patch("modules.cli.platform.system", return_value="Linux"),
            patch.dict(os.environ, {"SHELL": "/usr/bin/tcsh"}),
        ):
            result = CliRunner().invoke(completion, ["install"])
        assert result.exit_code != 0


class TestSubprocessCall:

    def _mock_run(self, shell, tmp_path, script):
        mock_run = MagicMock(return_value=_sp_result(script))
        with (
            patch("modules.cli.platform.system", return_value="Linux"),
            patch("subprocess.run", mock_run),
            patch("pathlib.Path.home", return_value=tmp_path),
            patch.object(sys, "executable", FAKE_PYTHON),
            patch.object(sys, "argv", [FAKE_SCRIPT]),
        ):
            CliRunner().invoke(completion, ["install", "--shell", shell])
        return mock_run

    def test_uses_sys_executable_directly_not_resolved(self, tmp_path):
        """Regression: Path(sys.executable).resolve() returned system Python without venv packages."""
        mock_run = self._mock_run("zsh", tmp_path, ZSH_CLICK_SCRIPT)
        cmd = mock_run.call_args[0][0]
        assert cmd[0] == FAKE_PYTHON

    def test_env_var_is_wifi_spectrum_complete(self, tmp_path):
        mock_run = self._mock_run("zsh", tmp_path, ZSH_CLICK_SCRIPT)
        env = mock_run.call_args[1]["env"]
        assert "_WIFI_SPECTRUM_COMPLETE" in env

    def test_zsh_uses_zsh_source_flag(self, tmp_path):
        mock_run = self._mock_run("zsh", tmp_path, ZSH_CLICK_SCRIPT)
        assert mock_run.call_args[1]["env"]["_WIFI_SPECTRUM_COMPLETE"] == "zsh_source"

    def test_bash_uses_bash_source_flag(self, tmp_path):
        mock_run = self._mock_run("bash", tmp_path, BASH_CLICK_SCRIPT)
        assert mock_run.call_args[1]["env"]["_WIFI_SPECTRUM_COMPLETE"] == "bash_source"

    def test_fish_uses_fish_source_flag(self, tmp_path):
        mock_run = self._mock_run("fish", tmp_path, FISH_CLICK_SCRIPT)
        assert mock_run.call_args[1]["env"]["_WIFI_SPECTRUM_COMPLETE"] == "fish_source"


class TestEmptyScriptHandling:

    def test_empty_stdout_exits_with_error(self, tmp_path):
        with (
            patch("modules.cli.platform.system", return_value="Linux"),
            patch("subprocess.run", return_value=_sp_result("")),
            patch("pathlib.Path.home", return_value=tmp_path),
            patch.object(sys, "executable", FAKE_PYTHON),
            patch.object(sys, "argv", [FAKE_SCRIPT]),
        ):
            result = CliRunner().invoke(completion, ["install", "--shell", "zsh"])
        assert result.exit_code != 0

    def test_whitespace_only_stdout_exits_with_error(self, tmp_path):
        with (
            patch("modules.cli.platform.system", return_value="Linux"),
            patch("subprocess.run", return_value=_sp_result("   \n  ")),
            patch("pathlib.Path.home", return_value=tmp_path),
            patch.object(sys, "executable", FAKE_PYTHON),
            patch.object(sys, "argv", [FAKE_SCRIPT]),
        ):
            result = CliRunner().invoke(completion, ["install", "--shell", "zsh"])
        assert result.exit_code != 0


class TestZshPostProcessing:

    def _script(self, tmp_path):
        _, script = _run_install("zsh", tmp_path, ZSH_CLICK_SCRIPT)
        return script

    def test_compdef_directive_includes_wifi_spectrum_py(self, tmp_path):
        assert "#compdef wifi-spectrum wifi_spectrum.py ./wifi_spectrum.py" in self._script(tmp_path)

    def test_path_guard_is_neutralized(self, tmp_path):
        script = self._script(tmp_path)
        assert "false && return 1" in script
        assert "$+commands[wifi-spectrum]" not in script

    def test_completion_call_uses_absolute_python_path(self, tmp_path):
        assert f"_WIFI_SPECTRUM_COMPLETE=zsh_complete {FAKE_PYTHON}" in self._script(tmp_path)

    def test_compdef_registration_includes_wifi_spectrum_py(self, tmp_path):
        assert "compdef _wifi_spectrum_completion wifi-spectrum wifi_spectrum.py" in self._script(tmp_path)

    def test_wrapper_function_uses_underscore_name(self, tmp_path):
        """Regression: wrapper used _wifi-spectrum_completion (hyphen), causing 'command not found'."""
        script = self._script(tmp_path)
        assert "_wifi_spectrum_completion_python_wrapper" in script
        assert "_wifi-spectrum_completion" not in script

    def test_wrapper_lazy_loads_completion_function(self, tmp_path):
        script = self._script(tmp_path)
        expected_source = str(tmp_path / ".zfunc" / "_wifi-spectrum")
        assert "$+functions[_wifi_spectrum_completion]" in script
        assert f"source {expected_source}" in script

    def test_wrapper_registered_for_python_and_python3(self, tmp_path):
        assert "compdef _wifi_spectrum_completion_python_wrapper python python3" in self._script(tmp_path)

    def test_wrapper_falls_back_to_file_completion(self, tmp_path):
        assert "_files" in self._script(tmp_path)

    def test_script_written_to_zfunc(self, tmp_path):
        result, _ = _run_install("zsh", tmp_path, ZSH_CLICK_SCRIPT)
        assert result.exit_code == 0
        assert (tmp_path / ".zfunc" / "_wifi-spectrum").exists()


class TestBashPostProcessing:

    def _script(self, tmp_path):
        _, script = _run_install("bash", tmp_path, BASH_CLICK_SCRIPT)
        return script

    def test_completion_call_uses_absolute_python_path(self, tmp_path):
        assert f"_WIFI_SPECTRUM_COMPLETE=bash_complete {FAKE_PYTHON}" in self._script(tmp_path)

    def test_complete_registration_includes_wifi_spectrum_py(self, tmp_path):
        assert "complete -F _wifi_spectrum_completion wifi-spectrum wifi-spectrum.py" in self._script(tmp_path)

    def test_wrapper_registered_for_python_and_python3(self, tmp_path):
        assert "complete -F _wifi_spectrum_completion_python_wrapper python python3" in self._script(tmp_path)

    def test_wrapper_falls_back_to_file_completion(self, tmp_path):
        assert 'COMPREPLY=( $(compgen -f -- "$cur") )' in self._script(tmp_path)

    def test_script_written_to_bash_completions(self, tmp_path):
        result, _ = _run_install("bash", tmp_path, BASH_CLICK_SCRIPT)
        assert result.exit_code == 0
        target = tmp_path / ".local" / "share" / "bash-completion" / "completions" / "wifi-spectrum"
        assert target.exists()


class TestFishPostProcessing:

    def test_completion_call_uses_absolute_python_path(self, tmp_path):
        _, script = _run_install("fish", tmp_path, FISH_CLICK_SCRIPT)
        assert f"_WIFI_SPECTRUM_COMPLETE=fish_complete {FAKE_PYTHON}" in script

    def test_script_written_to_fish_completions(self, tmp_path):
        result, _ = _run_install("fish", tmp_path, FISH_CLICK_SCRIPT)
        assert result.exit_code == 0
        target = tmp_path / ".config" / "fish" / "completions" / "wifi-spectrum.fish"
        assert target.exists()


class TestZshRcModification:

    def test_adds_fpath_entry_when_not_present(self, tmp_path):
        _run_install("zsh", tmp_path, ZSH_CLICK_SCRIPT)
        zshrc = tmp_path / ".zshrc"
        assert zshrc.exists()
        assert ".zfunc" in zshrc.read_text()

    def test_skips_modification_when_zfunc_already_present(self, tmp_path):
        zshrc = tmp_path / ".zshrc"
        zshrc.write_text("fpath=(~/.zfunc $fpath)\nautoload -Uz compinit\n")

        _run_install("zsh", tmp_path, ZSH_CLICK_SCRIPT)

        assert zshrc.read_text().count(".zfunc") == 1
