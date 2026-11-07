import importlib.util
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch


class SaveHookTests(unittest.TestCase):
    def test_successful_container_open_dispatch_is_optional(self):
        adapter = SimpleNamespace(source_container_open=Mock())
        with patch.dict(sys.modules, {'bg2toee_lifecycle': adapter}):
            self.hook.source_container_open('container', 'actor')
        adapter.source_container_open.assert_called_once_with(('container', 'actor'))
        with patch.dict(sys.modules, {'bg2toee_lifecycle': SimpleNamespace()}):
            self.hook.source_container_open('container', 'actor')

    def setUp(self):
        path = Path(__file__).resolve().parents[2] / 'tpdata/templeplus/lib/templeplus/savehook.py'
        spec = importlib.util.spec_from_file_location('savehook_under_test', path)
        self.hook = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.hook)

    def test_optional_adapter_absent_and_original_co8_post_hooks(self):
        self.hook.pre_load('legacy')
        self.assertTrue(self.hook.save('legacy'))
        self.hook.load('legacy')
        co8 = SimpleNamespace(save=Mock(), load=Mock())
        with patch.dict(sys.modules, {'_co8init': co8}):
            self.hook.post_save('slot1')
            self.hook.post_load('slot2')
        co8.save.assert_called_once_with('slot1')
        co8.load.assert_called_once_with('slot2')

    def test_adapter_save_failure_is_explicit_false_for_native_archive_guard(self):
        adapter = SimpleNamespace(save=Mock(side_effect=OSError('disk full')))
        with patch.dict(sys.modules, {'bg2toee_lifecycle': adapter}):
            self.assertIs(self.hook.save('existing_slot'), False)
        adapter.save.assert_called_once_with('existing_slot')

    def test_preload_save_and_load_dispatch_to_adapter(self):
        adapter = SimpleNamespace(pre_load=Mock(), save=Mock(), load=Mock())
        with patch.dict(sys.modules, {'bg2toee_lifecycle': adapter}):
            self.hook.pre_load('old')
            self.assertTrue(self.hook.save('new'))
            self.hook.load('new')
        adapter.pre_load.assert_called_once_with('old')
        adapter.save.assert_called_once_with('new')
        adapter.load.assert_called_once_with('new')

    def test_load_failure_is_not_silently_accepted(self):
        adapter = SimpleNamespace(load=Mock(side_effect=ValueError('corrupt session')))
        with patch.dict(sys.modules, {'bg2toee_lifecycle': adapter}):
            with self.assertRaisesRegex(ValueError, 'corrupt session'):
                self.hook.load('bad')

    def test_map_ready_and_door_trap_dispatch_or_ignore_old_adapter(self):
        adapter = SimpleNamespace(map_ready=Mock(), door_trap=Mock())
        with patch.dict(sys.modules, {'bg2toee_lifecycle': adapter}):
            self.hook.map_ready(5136)
            self.hook.door_trap(5136, 'DOOR2607', 'actor')
        adapter.map_ready.assert_called_once_with(5136)
        adapter.door_trap.assert_called_once_with((5136, 'DOOR2607', 'actor'))
        old = SimpleNamespace(load=Mock())
        with patch.dict(sys.modules, {'bg2toee_lifecycle': old}):
            self.hook.map_ready(5136)
            self.hook.door_trap(5136, 'DOOR2607', 'actor')
        self.hook.map_ready(5001)  # no adapter installed

    def test_map_ready_failure_is_logged_not_raised(self):
        adapter = SimpleNamespace(map_ready=Mock(side_effect=ValueError('stale data')))
        with patch.dict(sys.modules, {'bg2toee_lifecycle': adapter}):
            self.hook.map_ready(5136)


if __name__ == '__main__':
    unittest.main()
