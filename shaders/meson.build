shaders = ['texture.vert', 'texture.frag',
           'cube.vert', 'cube.frag',
           'toy.vert']

glslc = find_program('glslc', required : false)
if glslc.found()
  # Prefer shaderc
  cmd = [glslc]
else
  # Use glslang as fallback
  glslang = find_program('glslangValidator')
  if glslang.found()
    cmd = [glslang, '-V']
  endif
endif

foreach s : shaders
  r = run_command(cmd + ['-o', s + '.spv', s])
  if r.returncode() != 0
    message('Could not compile shaders:')
    message(r.stderr().strip())
    message(r.stdout().strip())
  endif
endforeach

shader_resources = gnome.compile_resources(
  'shader_resources', 'shaders.gresource.xml',
  source_dir : '.')
