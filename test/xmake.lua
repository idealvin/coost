function all_tests()
    local res = {}
    for _, x in ipairs(os.files("*_test.cc")) do
        x = path.filename(x)
        table.insert(res, x:sub(1, #x - 8))
    end
    return res
end

for _, test in ipairs(all_tests()) do
target(test)
    set_kind("binary")
    set_default(false)
    add_deps("base")
--  add_links("jemalloc")
    add_files(test .. "_test.cc")
end
