-- Loaded with --lua. Return source, or call domas.emit() and return nil.
domas.directive("u32", function(arguments, context)
    assert(context.target == domas.target)
    return ".4byte " .. arguments
end)

domas.directive("return_value", function(arguments)
    local n = assert(tonumber(arguments), "expected a number")
    if domas.target == "amd64" then
        if domas.syntax == "intel" then
            return "mov eax, " .. n .. "\nret\n"
        end
        return "mov $" .. n .. ", %eax\nret\n"
    elseif domas.target == "aarch64" then
        return "mov w0, #" .. n .. "\nret\n"
    elseif domas.target == "riscv64" then
        return "li a0, " .. n .. "\nret\n"
    end
    error("return_value has no lowering for " .. domas.target)
end)
