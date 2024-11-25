local M = {}

function M.build_and_run()
  -- Define the output file for nasm
  local asm_output = 'out.bin'

  -- Run nasm to assemble your .asm file
  vim.fn.system('nasm -f bin -o ' .. asm_output .. ' simple_bootsector.asm')

  -- Check if nasm succeeded
  if vim.v.shell_error ~= 0 then
    vim.api.nvim_echo({{"NASM failed to assemble the file!", "ErrorMsg"}}, true, {})
    return
  end

  -- Run the output file with QEMU
  vim.fn.system('qemu-system-x86_64 ' .. asm_output)
end

print("Welcome to the os")

-- Create a Neovim command that calls this function
vim.api.nvim_create_user_command("BuildProject", M.build_and_run, {})

--vim.keybinds.set('n', '<leader>bp', M.build_and_run, {})

return M

