from hatchling.builders.hooks.plugin.interface import BuildHookInterface
import sysconfig

class CustomHook(BuildHookInterface):
    def initialize(self, version, build_data):
        build_data['pure_python'] = False
        platform = sysconfig.get_platform().replace('-', '_').replace('.', '_')
        build_data['tag'] = f'py3-none-{platform}'
