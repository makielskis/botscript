{
  "variables" : {
    "boost_path": "/home/wasabi/boost_1_52_0/",
    "build_path": "/home/wasabi/Development/cppbot/botscript_new/"
  },
  "targets": [
    {
      "target_name": "addon",
      "include_dirs": [
        "<(boost_path)",
        "external_lib/rapidjson-r64/include",
        "external_lib/lua-5.2.0/src",
        "external_lib/tidy/include",
        "external_lib/pugixml-1.0/src"
      ],
      "ldflags": [
        "-L <(build_path)",
        "-L <(boost_path)stage/lib/"
      ],
      "link_settings": {
        "libraries": [
          "-lboost_system",
          "-lboost_thread",
          "-lboost_regex",
          "-lboost_iostreams",
          "-lboost_filesystem",
          "-ltidy",
          "-lpugixml",
          "-llua",
          "-lz"
        ]
      },
      "cflags!": [ "-fno-exceptions", "-fno-rtti" ],
      "cflags_cc!": [ "-fno-exceptions", "-fno-rtti" ],
      "cflags": [ "-std=c++0x" ],
      "sources": [
        "src/http/url.cc",
        "src/http/http_source.cc",
        "src/http/http_con.cc",
        "src/http/util.cc",
        "src/http/webclient.cc",
        "src/lua/lua_connection.cc",
        "src/lua/lua_util.cc",
        "src/lua/lua_http.cc",
        "src/bot_browser.cc",
        "src/module.cc",
        "src/bot.cc",
        "src/node/nodejs_botscript.cc"
      ]
    }
  ]
}

