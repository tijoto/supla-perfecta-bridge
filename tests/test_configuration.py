import copy, importlib.util, json, tempfile, unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('configuration',ROOT/'tools/configure.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
audit_spec=importlib.util.spec_from_file_location('audit',ROOT/'tools/audit_public.py');audit=importlib.util.module_from_spec(audit_spec);audit_spec.loader.exec_module(audit)
class ConfigurationTests(unittest.TestCase):
    def example(self):return json.loads((ROOT/'config.example.json').read_text())
    def valid(self):
        c=self.example();c['wifi']['ssid']='test-network-only';c['web']['password']='test-password-not-for-device';c['perfecta']['key']='0123456789ABCDEF';return c
    def test_defaults_are_empty_and_read_only(self):
        c=self.example();self.assertEqual(c['perfecta']['key'],'');self.assertFalse(c['controls']['enabled']);self.assertFalse(c['perfecta']['configuration_writes']);self.assertFalse(c['bridge']['enabled']);self.assertFalse(c['supla']['enabled'])
    def test_unfilled_example_cannot_be_built_as_installation(self):
        with self.assertRaises(ValueError):module.validate(self.example(),False)
    def test_identity_is_created_once_and_preserved(self):
        c=self.valid();module.validate(c,False);identity=(c['supla']['guid'],c['supla']['authkey']);module.validate(c,False);self.assertEqual(identity,(c['supla']['guid'],c['supla']['authkey']));self.assertNotEqual(*identity)
    def test_bad_key_and_password_are_rejected(self):
        for key in ['','short','a'*17,'ż'*16]:
            c=self.valid();c['perfecta']['key']=key
            with self.assertRaises(ValueError):module.validate(c,False)
        c=self.valid();c['web']['password']='short'
        with self.assertRaises(ValueError):module.validate(c,False)
    def test_boolean_flags_are_strict(self):
        c=self.valid();c['controls']['enabled']='false'
        with self.assertRaises(ValueError):module.validate(c,False)
    def test_enabled_bridge_requires_a_local_peer(self):
        c=self.valid();c['bridge']['enabled']=True;c['bridge']['allowed_client_ipv4']='203.0.113.8'
        with self.assertRaises(ValueError):module.validate(c,False)
    def test_supla_requires_certificate_configuration(self):
        c=self.valid();c['supla']['enabled']=True
        with self.assertRaises(ValueError):module.validate(c,False)
    def test_local_headers_are_separate_from_source(self):
        c=self.valid();cert,pin=module.validate(c,False);old=module.ROOT
        try:
            with tempfile.TemporaryDirectory() as folder:
                module.ROOT=Path(folder);module.write_headers(c,cert,pin,False)
                header=(module.ROOT/'firmware-esp32/include/private_config.h').read_text()
                self.assertIn('CONTROL_ENABLED=false',header);self.assertIn('BUILD_TEST_ONLY=false',header)
                module.write_headers(c,cert,pin,True);self.assertIn('BUILD_TEST_ONLY=true',(module.ROOT/'firmware-esp32/include/private_config.h').read_text())
        finally:module.ROOT=old
    def test_public_export(self):self.assertTrue(audit.audit(ROOT)['passed'])
if __name__=='__main__':unittest.main()
