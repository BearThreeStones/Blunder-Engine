from pathlib import Path

p = Path("engine/src/runtime/resource/asset_import/asset_import_service.cpp")
text = p.read_text(encoding="utf-8")
nl = "\r\n" if "\r\n" in text else "\n"

old_start = text.find("bool registerCompanionAnimationIntermediates(")
old_end = text.find("eastl::string archiveSourceAsset(", old_start)
assert old_start > 0 and old_end > old_start

new_fn = f"""bool registerCompanionAnimationIntermediates(
    FileSystem* file_system, const eastl::string& /*host_resource_virtual*/,
    const std::vector<fs::path>& companion_inputs,
    eastl::vector<eastl::string>& out_virtual_paths,
    std::vector<fs::path>& out_absolute_paths) {{
  out_virtual_paths.clear();
  out_absolute_paths.clear();
  if (companion_inputs.empty()) {{
    return true;
  }}

  // ADR 0028: companion exchange bodies live under Resources/Animations/<stem>/
  // (organization only — not Models/{{host}}/companions/).
  std::vector<fs::path> copied_paths;
  const auto fail = [&]() {{
    for (const fs::path& copied : copied_paths) {{
      std::error_code ec;
      fs::remove(copied, ec);
    }}
    out_virtual_paths.clear();
    out_absolute_paths.clear();
    return false;
  }};

  for (const fs::path& input : companion_inputs) {{
    const eastl::string resource_virtual =
        registerIntermediateBody(file_system, input, "Animations");
    if (resource_virtual.empty()) {{
      return fail();
    }}
    const fs::path destination_absolute =
        resolveResourcesVirtualPath(file_system, resource_virtual);
    if (!pathsReferToSameFile(input, destination_absolute)) {{
      copied_paths.push_back(destination_absolute);
    }}
    if (!copyGltfExternalResources(file_system, input, destination_absolute,
                                   copied_paths)) {{
      return fail();
    }}
    out_virtual_paths.push_back(resource_virtual);
    out_absolute_paths.push_back(destination_absolute);
  }}
  return true;
}}

"""
text = text[:old_start] + new_fn.replace("\n", nl) + text[old_end:]

text = text.replace(
    f"  descriptor.source = resource_virtual_path;{nl}"
    f"  descriptor.companion_animation_sources ={nl}"
    f"      companion_resource_virtual_paths;{nl}"
    f"  descriptor.import = settings;{nl}",
    f"  descriptor.source = resource_virtual_path;{nl}"
    f"  descriptor.import = settings;{nl}",
    1,
)

old_extract = (
    f"              companion_absolute, stem, make_name, companion_stem,{nl}"
    f"              assets_folder);"
)
new_extract = (
    f"              companion_absolute, companion_stem, make_name, companion_stem,{nl}"
    f"              assets_folder);"
)
assert old_extract in text, "extract call missing"
text = text.replace(old_extract, new_extract, 1)

text = text.replace('"Models/_standalone_companions"', '"Animations"')

old_re = f"""  refreshAnimationClipsFromGltf(file_system, asset_registry, content_browser,
                                gltf_absolute, mesh_stem, existing_clips,
                                make_unique_descriptor_name);

  for (const eastl::string& companion_source :
       mesh.companion_animation_sources) {{
    const eastl::string companion_stem =
        stemFromVirtualPath(companion_source);
    if (companion_source.empty() || companion_stem.empty()) {{
      continue;
    }}

    const fs::path companion_absolute =
        resolveResourcesVirtualPath(file_system, companion_source);
    warnOnCompanionAnimationBoneMismatches(gltf_absolute,
                                           companion_absolute);
    refreshAnimationClipsFromGltf(
        file_system, asset_registry, content_browser, companion_absolute,
        mesh_stem, existing_clips, make_unique_descriptor_name,
        companion_stem);
  }}
}}
"""
new_re = f"""  // ADR 0028: Mesh Reimport refreshes embedded clips only — companion Clips
  // Reimport from their own descriptor source / Animations/<stem>/ glTF.
  refreshAnimationClipsFromGltf(file_system, asset_registry, content_browser,
                                gltf_absolute, mesh_stem, existing_clips,
                                make_unique_descriptor_name);
  (void)mesh.companion_animation_sources;
}}
"""
assert old_re.replace("\n", nl) in text or old_re in text
text = text.replace(old_re.replace("\n", nl), new_re.replace("\n", nl), 1)

old_del = f"""      if (!descriptor.source.empty()) {{
        intermediate_virtuals.push_back(descriptor.source);
      }}
      for (const eastl::string& companion :
           descriptor.companion_animation_sources) {{
        if (!companion.empty()) {{
          intermediate_virtuals.push_back(companion);
        }}
      }}
"""
new_del = f"""      if (!descriptor.source.empty()) {{
        intermediate_virtuals.push_back(descriptor.source);
      }}
      // ADR 0028: companion Intermediate belongs to Clip Assets — do not
      // cascade-delete via Mesh packaging lists.
"""
assert old_del.replace("\n", nl) in text
text = text.replace(old_del.replace("\n", nl), new_del.replace("\n", nl), 1)

p.write_bytes(text.encode("utf-8"))
print("patched asset_import_service.cpp")
