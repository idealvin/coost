
for _, test in ipairs({
    "co", "fast", "flag", "hash", "json", "rapidjson", "log",
    "rpc", "stack", "str", "task_sched", "time", "tw", "xx"
}) do
target(test)
    set_kind("binary")
    set_default(false)
    add_deps("base")
--  add_links("jemalloc")
    add_files(test .. "_test.cc")
end

