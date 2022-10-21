import json
import platform
import subprocess
import unittest.mock as mock

import pytest

import parasoft_cpptestcli

from parasoft_cpptestcli import (
    CpptestcliError,
    CpptestDiagnostic,
    CpptestPosition,
    iter_sarif_result,
    run_cpptestcli,
    run_cpptestcli_process,
)


@pytest.fixture
def sarif():
    return {
        "$schema": "https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/Schemata/sarif-schema-2.1.0.json",
        "version": "2.1.0",
        "runs": [
            {
                "tool": {
                    "driver": {
                        "name": "C/C++test",
                        "semanticVersion": "2022.1.0",
                        "rules": [
                            {
                                "id": "rule1",
                                "name": "test rule1",
                                "shortDescription": {
                                    "text": "short description of rule1",
                                },
                                "fullDescription": {
                                    "text": "full description of rule1",
                                },
                                "defaultConfiguration": {
                                    "level": "error",
                                },
                                "help": {
                                    "text": "help message of rule1",
                                },
                                "properties": {
                                    "tags": [
                                        "rule1 tag1",
                                        "rule1 tag2",
                                    ],
                                },
                            },
                        ],
                    },
                },
                "results": [
                    {
                        "ruleId": "rule1",
                        "level": "warning",
                        "message": {
                            "text": "violation of the rule rule1",
                        },
                        "partialFingerprints": {
                            "lineHash": 1,
                        },
                        "locations": [
                            {
                                "physicalLocation": {
                                    "artifactLocation": {
                                        "uri": f"file://{platform.node()}/test/path/bad.c",
                                    },
                                },
                                "region": {
                                    "startLine": 30,
                                    "startColumn": 29,
                                    "endLine": 31,
                                    "endColumn": 3,
                                },
                            },
                        ],
                    },
                ],
            },
        ],
    }


@pytest.fixture
def cpptestdiagnostic():
    return CpptestDiagnostic(
        path="/test/path/bad.c",
        rule="rule1",
        message="violation of the rule rule1",
        start=CpptestPosition(
            line=30,
            column=29,
        ),
        end=CpptestPosition(
            line=31,
            column=3,
        ),
    )


class TestRunCpptestcli:
    @mock.patch.object(subprocess, "run")
    def test_run_cpptestcli_ia32(self, run):
        run.return_value.returncode = 0
        run.return_value.stdout = "test output"

        out = run_cpptestcli("ia32-generic-qemu", "compile_commands_abc.json", ["def.c"])

        cmd = [
            "cpptestcli",
            "-config",
            "user://MISRA C 2012",
            "-input",
            "compile_commands_abc.json",
            "-module",
            ".",
            "-compiler",
            "gcc_9",
            "-resource",
            "def.c",
        ]

        run.assert_called_once_with(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding="utf-8")
        assert out == (run.return_value.returncode, run.return_value.stdout)

    @mock.patch.object(subprocess, "run")
    def test_run_cpptestcli_arm(self, run):
        run.return_value.returncode = 0
        run.return_value.stdout = "test output"

        out = run_cpptestcli("armv7a7-imx6ull-evk", "compile_commands_123.json", ["456.c"])

        cmd = [
            "cpptestcli",
            "-config",
            "user://MISRA C 2012",
            "-input",
            "compile_commands_123.json",
            "-module",
            ".",
            "-compiler",
            "gcc_9_ARM",
            "-resource",
            "456.c",
        ]

        run.assert_called_once_with(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding="utf-8")
        assert out == (run.return_value.returncode, run.return_value.stdout)

    @mock.patch.object(subprocess, "run")
    def test_run_cpptestcli_multi_file(self, run):
        run.return_value.returncode = 0
        run.return_value.stdout = "test output"

        out = run_cpptestcli("armv7a7-imx6ull-evk", "compile_commands.json", ["1.c", "2.c", "3.c"])

        cmd = [
            "cpptestcli",
            "-config",
            "user://MISRA C 2012",
            "-input",
            "compile_commands.json",
            "-module",
            ".",
            "-compiler",
            "gcc_9_ARM",
            "-resource",
            "1.c",
            "-resource",
            "2.c",
            "-resource",
            "3.c",
        ]

        run.assert_called_once_with(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding="utf-8")
        assert out == (run.return_value.returncode, run.return_value.stdout)

    @mock.patch.object(subprocess, "run")
    def test_run_cpptestcli_not_found(self, run):
        run.side_effect = FileNotFoundError

        with pytest.raises(CpptestcliError):
            run_cpptestcli("armv7a7-imx6ull-evk", "compile_commands.json", ["a.c"])

        cmd = [
            "cpptestcli",
            "-config",
            "user://MISRA C 2012",
            "-input",
            "compile_commands.json",
            "-module",
            ".",
            "-compiler",
            "gcc_9_ARM",
            "-resource",
            "a.c",
        ]

        run.assert_called_once_with(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding="utf-8")

    @mock.patch.object(subprocess, "run")
    def test_run_cpptestcli_rc_non_zero(self, run):
        run.return_value.returncode = 1
        run.return_value.stdout = "test output"

        # Check if expection is not raised
        out = run_cpptestcli("armv7a7-imx6ull-evk", "compile_commands.json", ["a.c"])
        run.assert_called_once()
        assert out == (run.return_value.returncode, run.return_value.stdout)


class TestRunCpptestcliPorcess:
    @mock.patch.object(parasoft_cpptestcli, "run_cpptestcli")
    def test_rc_non_zero(self, run_cpptestcli):
        run_cpptestcli.return_value = (1, "output")

        with pytest.raises(CpptestcliError):
            run_cpptestcli_process("armv7a7-imx6ull-evk", "compile_commands.json", ["a.c"])

    @mock.patch.object(parasoft_cpptestcli, "run_cpptestcli")
    @mock.patch.object(parasoft_cpptestcli, "iter_sarif_result")
    @mock.patch("builtins.open", new_callable=mock.mock_open, read_data='{"json":"data"}')
    def test_mocked_run_cpptestcli_process(self, mock_file, iter_sarif_result, run_cpptestcli):
        run_cpptestcli.return_value = (0, "ok")

        out = run_cpptestcli_process("ia32-generic-qemu", "compile_commands.json", ["a.c", "b.c", "c.c"])

        run_cpptestcli.assert_called_once_with("ia32-generic-qemu", "compile_commands.json", ["a.c", "b.c", "c.c"])
        mock_file.assert_called_once_with("reports/report.sarif", "r")
        iter_sarif_result.assert_called_once_with({"json": "data"})
        assert out is iter_sarif_result.return_value

    @mock.patch.object(parasoft_cpptestcli, "run_cpptestcli")
    def test_run_cpptestcli_process(self, run_cpptestcli, sarif, cpptestdiagnostic):
        run_cpptestcli.return_value = (0, "ok")
        raw_sarif = json.dumps(sarif)

        with mock.patch("builtins.open", mock.mock_open(read_data=raw_sarif)) as mock_file:
            generator = run_cpptestcli_process("ia32-generic-qemu", "compile_commands.json", ["a.c"])
            mock_file.assert_called_once_with("reports/report.sarif", "r")
            assert next(generator) == cpptestdiagnostic


class TestIterSarifResult:
    @staticmethod
    def get_location(data):
        return TestIterSarifResult.get_result(data)["locations"][0]

    @staticmethod
    def get_driver(data):
        return TestIterSarifResult.get_run(data)["tool"]["driver"]

    @staticmethod
    def get_result(data):
        return TestIterSarifResult.get_run(data)["results"][0]

    @staticmethod
    def get_run(data):
        return data["runs"][0]

    def test_empty_runs(self, sarif):
        sarif["runs"].clear()
        with pytest.raises(CpptestcliError):
            next(iter_sarif_result(sarif))

    def test_no_runs(self, sarif):
        del sarif["runs"]
        with pytest.raises(CpptestcliError):
            next(iter_sarif_result(sarif))

    def test_no_results(self, sarif):
        run = self.get_run(sarif)
        del run["results"]

        with pytest.raises(CpptestcliError):
            next(iter_sarif_result(sarif))

    def test_bad_name(self, sarif):
        driver = self.get_driver(sarif)
        driver["name"] = "bad tool name"
        with pytest.raises(CpptestcliError):
            next(iter_sarif_result(sarif))

    def test_bad_version(self, sarif):
        driver = self.get_driver(sarif)
        driver["semanticVersion"] = "bad sem version"
        with pytest.raises(CpptestcliError):
            next(iter_sarif_result(sarif))

    def test_location_no_uri(self, sarif):
        location = self.get_location(sarif)
        del location["physicalLocation"]["artifactLocation"]["uri"]
        with pytest.raises(CpptestcliError):
            next(iter_sarif_result(sarif))

    def test_location_no_artifact_location(self, sarif):
        location = self.get_location(sarif)
        del location["physicalLocation"]["artifactLocation"]
        with pytest.raises(CpptestcliError):
            next(iter_sarif_result(sarif))

    def test_empty_locations(self, sarif):
        result = self.get_result(sarif)
        result["locations"].clear()
        with pytest.raises(CpptestcliError):
            next(iter_sarif_result(sarif))

    def test_no_locations(self, sarif):
        result = self.get_result(sarif)
        del result["locations"]
        with pytest.raises(CpptestcliError):
            next(iter_sarif_result(sarif))

    def test_empty_region(self, sarif):
        location = self.get_location(sarif)
        location["region"].clear()

        with pytest.raises(CpptestcliError):
            next(iter_sarif_result(sarif))

    def test_no_region(self, sarif):
        location = self.get_location(sarif)
        del location["region"]
        with pytest.raises(CpptestcliError):
            next(iter_sarif_result(sarif))

    def test_no_start_column(self, sarif):
        region = self.get_location(sarif)["region"]
        del region["startColumn"]

        output = CpptestDiagnostic(
            path="/test/path/bad.c",
            rule="rule1",
            message="violation of the rule rule1",
            start=CpptestPosition(
                line=30,
                column=None,
            ),
            end=CpptestPosition(
                line=31,
                column=3,
            ),
        )

        assert next(iter_sarif_result(sarif)) == output

    def test_no_end_line(self, sarif):
        region = self.get_location(sarif)["region"]
        del region["endLine"]

        output = CpptestDiagnostic(
            path="/test/path/bad.c",
            rule="rule1",
            message="violation of the rule rule1",
            start=CpptestPosition(
                line=30,
                column=29,
            ),
            end=CpptestPosition(
                line=None,
                column=3,
            ),
        )

        assert next(iter_sarif_result(sarif)) == output

    def test_no_end_column(self, sarif):
        region = self.get_location(sarif)["region"]
        del region["endColumn"]

        output = CpptestDiagnostic(
            path="/test/path/bad.c",
            rule="rule1",
            message="violation of the rule rule1",
            start=CpptestPosition(
                line=30,
                column=29,
            ),
            end=CpptestPosition(line=31, column=None),
        )

        assert next(iter_sarif_result(sarif)) == output

    def test_no_end_column_and_line(self, sarif):
        region = self.get_location(sarif)["region"]
        del region["endColumn"]
        del region["endLine"]

        output = CpptestDiagnostic(
            path="/test/path/bad.c",
            rule="rule1",
            message="violation of the rule rule1",
            start=CpptestPosition(
                line=30,
                column=29,
            ),
            end=None,
        )

        assert next(iter_sarif_result(sarif)) == output

    def test_code_flow(self):
        pass

    def test_one_diagnostic(self, sarif, cpptestdiagnostic):
        assert next(iter_sarif_result(sarif)) == cpptestdiagnostic

    def test_few_diagnostics(self, sarif, cpptestdiagnostic):
        results = self.get_run(sarif)["results"]
        expected = [cpptestdiagnostic]

        results.append(
            {
                "ruleId": "r2",
                "level": "warning",
                "message": {"text": "text2"},
                "partialFingerprints": {"lineHash": 1},
                "locations": [
                    {
                        "physicalLocation": {"artifactLocation": {"uri": f"file://{platform.node()}/test/path/r2.c"}},
                        "region": {"startLine": 1},
                    },
                ],
            }
        )
        expected.append(
            CpptestDiagnostic(
                path="/test/path/r2.c",
                rule="r2",
                message="text2",
                start=CpptestPosition(
                    line=1,
                    column=None,
                ),
                end=None,
            )
        )

        results.append(
            {
                "ruleId": "r3",
                "level": "warning",
                "message": {"text": "text3"},
                "partialFingerprints": {"lineHash": 1},
                "locations": [
                    {
                        "physicalLocation": {"artifactLocation": {"uri": f"file://{platform.node()}/test/path/r3.c"}},
                        "region": {"startLine": 1, "endLine": 2},
                    },
                ],
            }
        )
        expected.append(
            CpptestDiagnostic(
                path="/test/path/r3.c",
                rule="r3",
                message="text3",
                start=CpptestPosition(
                    line=1,
                    column=None,
                ),
                end=CpptestPosition(
                    line=2,
                    column=None,
                ),
            )
        )

        for result, expect in zip(iter_sarif_result(sarif), expected):
            assert result == expect
