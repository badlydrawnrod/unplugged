"""Exercise the repository verification entry point as a public command."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


VERIFY_SCRIPT = Path(__file__).resolve().parents[1] / "verify.py"


class VerifyIntegrationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "checkout"
        (self.root / "tools/acceptance").mkdir(parents=True)
        (self.root / "tools/architecture").mkdir(parents=True)
        shutil.copy2(VERIFY_SCRIPT, self.root / "tools/verify.py")
        self.log = Path(self.temporary.name) / "calls.jsonl"
        self.clang_format = Path(self.temporary.name) / "clang-format-fixture"

        self.write_child("tools/check_format.py", "format", "FAIL_FORMAT")
        self.write_child("tools/acceptance/check_traceability.py", "acceptance", "FAIL_ACCEPTANCE")
        self.write_child("tools/acceptance/check_traceability_tests.py", "acceptance_tests")
        self.write_child("tools/architecture/check.py", "checker", "CHECK_STATUS")
        (self.root / "tools/architecture/fixture_tests.py").write_text(
            "import os\nimport unittest\nfrom pathlib import Path\n\n"
            "class FixtureTest(unittest.TestCase):\n"
            "    def test_suite_runs(self):\n"
            "        with Path(os.environ['VERIFY_LOG']).open('a') as log:\n"
            "            log.write('suite\\n')\n"
            "        self.assertNotEqual(os.environ.get('FAIL_ARCH_TEST'), '1')\n\n"
            "if __name__ == '__main__':\n"
            "    unittest.main()\n"
        )

    def write_child(self, relative, name, failure_flag=None):
        source = (
            "import json, os, sys\n"
            "from pathlib import Path\n"
            f"event = {{'name': {name!r}, 'cwd': str(Path.cwd()), 'args': sys.argv[1:]}}\n"
            "with Path(os.environ['VERIFY_LOG']).open('a') as log:\n"
            "    log.write(json.dumps(event) + '\\n')\n"
        )
        if name == "checker":
            source += "print('checker fixture status', os.environ['CHECK_STATUS'])\n"
            source += "sys.exit(int(os.environ['CHECK_STATUS']))\n"
        elif failure_flag:
            source += f"sys.exit(1 if os.environ.get({failure_flag!r}) == '1' else 0)\n"
        else:
            source += "sys.exit(0)\n"
        path = self.root / relative
        path.write_text(source)

    def invoke(self, *, clang_format=None, **extra_env):
        env = os.environ.copy()
        env.update({"VERIFY_LOG": str(self.log), "CHECK_STATUS": "0"})
        env.update(extra_env)
        args = [sys.executable, str(self.root / "tools/verify.py")]
        if clang_format is not None:
            args.extend(["--clang-format", str(clang_format)])
        outside = Path(self.temporary.name)
        return subprocess.run(args, cwd=outside, env=env, text=True, capture_output=True)

    def events(self):
        return [json.loads(line) for line in self.log.read_text().splitlines() if line.startswith("{")]

    def test_checker_violation_and_operational_statuses_fail_verification(self):
        for checker_status in (1, 2):
            with self.subTest(checker_status=checker_status):
                self.log.unlink(missing_ok=True)
                result = self.invoke(CHECK_STATUS=str(checker_status))
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn(f"checker fixture status {checker_status}", result.stdout)
                names = [event["name"] for event in self.events()]
                self.assertEqual(names, ["format", "acceptance", "acceptance_tests", "checker"])
                self.assertIn("suite", self.log.read_text())

    def test_architecture_suite_failure_fails_verification(self):
        result = self.invoke(FAIL_ARCH_TEST="1")
        self.assertEqual(result.returncode, 1)
        self.assertIn("FAILED (failures=1)", result.stderr)
        names = [event["name"] for event in self.events()]
        self.assertEqual(names, ["format", "acceptance", "acceptance_tests", "checker"])
        self.assertIn("suite", self.log.read_text())

    def test_prior_failures_are_aggregated_and_later_checks_still_run(self):
        result = self.invoke(FAIL_FORMAT="1", FAIL_ACCEPTANCE="1")
        self.assertEqual(result.returncode, 1)
        self.assertEqual(
            [event["name"] for event in self.events()],
            ["format", "acceptance", "acceptance_tests", "checker"],
        )
        self.assertIn("suite", self.log.read_text())

    def test_script_root_and_clang_format_argument_work_outside_repository(self):
        result = self.invoke(clang_format=self.clang_format)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        events = self.events()
        self.assertEqual(events[0]["args"], ["--clang-format", str(self.clang_format)])
        self.assertTrue(all(event["cwd"] == str(self.root) for event in events))
        self.assertIn("suite", self.log.read_text())


if __name__ == "__main__":
    unittest.main()
