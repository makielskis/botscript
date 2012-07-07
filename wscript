BOOST_PATH = '/home/wasabi/boost_1_48_0/'
BUILD_PATH = '/home/wasabi/Development/cppbot/botscript_new/'

def set_options(ctx):
	ctx.tool_options('compiler_cxx')

def configure(ctx):
	ctx.check_tool('compiler_cxx')
	ctx.check_tool('node_addon')
	ctx.env.append_value('LINKFLAGS', ['-L' + BUILD_PATH, '-L' + BOOST_PATH + '/stage/lib/', '-lboost_system', '-lboost_thread', '-lboost_regex', '-lboost_iostreams', '-lboost_filesystem', '-pthread', '-ltidy', '-lpugixml', '-llua', '-lz'])

def build(ctx):
	t = ctx.new_task_gen('cxx', 'shlib', 'node_addon')
	t.target = 'addon'
	t.source = 'src/node/nodejs_botscript.cc src/lua_connection.cpp src/bot.cpp src/module.cpp'
	t.includes = BOOST_PATH + ' ' + BUILD_PATH + 'external_lib/rapidjson-r64/include ' + BUILD_PATH + 'external_lib/lua-5.2.0/src ' + BUILD_PATH + 'external_lib/tidy/include ' + BUILD_PATH + 'external_lib/pugixml-1.0/src'
	t.cxxflags = '-g'
