from pathlib import Path
import tempfile
import unittest

from easy_delay.config import create_example_config, load_config


class ConfigTest(unittest.TestCase):
    def test_defaults(self) -> None:
        content = '[target]\nhost="192.0.2.1"\nuser="tester"\npassword="secret"\n'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.toml"
            path.write_text(content)
            config = load_config(path)
        self.assertEqual(config.target.port, 49220)
        self.assertEqual(config.measurement.threshold_ms, 50.0)

    def test_rejects_relative_remote_directory(self) -> None:
        content = '[target]\nhost="192.0.2.1"\nuser="tester"\npassword="secret"\nremote_directory="tmp/x"\n'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.toml"
            path.write_text(content)
            with self.assertRaises(ValueError):
                load_config(path)

    def test_requires_password_authentication(self) -> None:
        content = '[target]\nhost="192.0.2.1"\nuser="tester"\n'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.toml"
            path.write_text(content)
            with self.assertRaises(ValueError):
                load_config(path)

    def test_creates_loadable_example(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "easy-delay.toml"
            create_example_config(path)
            config = load_config(path)
        self.assertEqual(config.target.password, "your-password")


if __name__ == "__main__":
    unittest.main()
