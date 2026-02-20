#pragma once

#include <vector>
#include <filesystem>
#include <cstdint>
#include <algorithm>
#include "../../system/renderer.h"

#include "assimpscenemeshextracter.h"

std::vector<MATERIAL> BuildLegacyMaterialVector(
    const std::vector<AssimpSceneMeshExtractor::MaterialData>& mtrls);
