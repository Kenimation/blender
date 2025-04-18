/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#include "internal/base/type_convert.h"
#include "internal/topology/mesh_topology.h"

#include "opensubdiv_converter_capi.hh"
#include "opensubdiv_topology_refiner.hh"

namespace blender::opensubdiv {

static const OpenSubdiv::Far::TopologyRefiner *getOSDTopologyRefiner(
    const TopologyRefinerImpl *topology_refiner_impl)
{
  return topology_refiner_impl->topology_refiner;
}

static const OpenSubdiv::Far::TopologyLevel &getOSDTopologyBaseLevel(
    const TopologyRefinerImpl *topology_refiner_impl)
{
  return getOSDTopologyRefiner(topology_refiner_impl)->GetLevel(0);
}

////////////////////////////////////////////////////////////////////////////////
// Quick preliminary checks.

static bool checkSchemeTypeMatches(const TopologyRefinerImpl *topology_refiner_impl,
                                   const OpenSubdiv_Converter *converter)
{
  const OpenSubdiv::Sdc::SchemeType converter_scheme_type =
      blender::opensubdiv::getSchemeTypeFromCAPI(converter->getSchemeType(converter));
  return (converter_scheme_type == getOSDTopologyRefiner(topology_refiner_impl)->GetSchemeType());
}

static bool checkOptionsMatches(const TopologyRefinerImpl *topology_refiner_impl,
                                const OpenSubdiv_Converter *converter)
{
  using Options = OpenSubdiv::Sdc::Options;
  const Options options = getOSDTopologyRefiner(topology_refiner_impl)->GetSchemeOptions();
  const Options::FVarLinearInterpolation fvar_interpolation = options.GetFVarLinearInterpolation();
  const Options::FVarLinearInterpolation converter_fvar_interpolation =
      blender::opensubdiv::getFVarLinearInterpolationFromCAPI(
          converter->getFVarLinearInterpolation(converter));
  if (fvar_interpolation != converter_fvar_interpolation) {
    return false;
  }
  return true;
}

static bool checkPreliminaryMatches(const TopologyRefinerImpl *topology_refiner_impl,
                                    const OpenSubdiv_Converter *converter)
{
  return checkSchemeTypeMatches(topology_refiner_impl, converter) &&
         checkOptionsMatches(topology_refiner_impl, converter);
}

////////////////////////////////////////////////////////////////////////////////
// Compare attributes which affects on topology.
//
// TODO(sergey): Need to look into how auto-winding affects on face-varying
// indexing and, possibly, move to mesh topology as well if winding affects
// face-varyign as well.

static bool checkSingleUVLayerMatch(const OpenSubdiv::Far::TopologyLevel &base_level,
                                    const OpenSubdiv_Converter *converter,
                                    const int layer_index)
{
  converter->precalcUVLayer(converter, layer_index);
  const int num_faces = base_level.GetNumFaces();
  // TODO(sergey): Need to check whether converter changed the winding of
  // face to match OpenSubdiv's expectations.
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    OpenSubdiv::Far::ConstIndexArray base_level_face_uvs = base_level.GetFaceFVarValues(
        face_index, layer_index);
    for (int corner = 0; corner < base_level_face_uvs.size(); ++corner) {
      const int uv_index = converter->getFaceCornerUVIndex(converter, face_index, corner);
      if (base_level_face_uvs[corner] != uv_index) {
        converter->finishUVLayer(converter);
        return false;
      }
    }
  }
  converter->finishUVLayer(converter);
  return true;
}

static bool checkUVLayersMatch(const TopologyRefinerImpl *topology_refiner_impl,
                               const OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Far::TopologyLevel;
  const int num_layers = converter->getNumUVLayers(converter);
  const TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner_impl);
  // Number of UV layers should match.
  if (base_level.GetNumFVarChannels() != num_layers) {
    return false;
  }
  for (int layer_index = 0; layer_index < num_layers; ++layer_index) {
    if (!checkSingleUVLayerMatch(base_level, converter, layer_index)) {
      return false;
    }
  }
  return true;
}

static bool checkTopologyAttributesMatch(const TopologyRefinerImpl *topology_refiner_impl,
                                         const OpenSubdiv_Converter *converter)
{
  return checkUVLayersMatch(topology_refiner_impl, converter);
}

bool TopologyRefinerImpl::isEqualToConverter(const OpenSubdiv_Converter *converter) const
{
  if (!blender::opensubdiv::checkPreliminaryMatches(this, converter)) {
    return false;
  }

  if (!base_mesh_topology.isEqualToConverter(converter)) {
    return false;
  }

  // NOTE: Do after geometry check, to be sure topology does match and all
  // indexing will go fine.
  if (!blender::opensubdiv::checkTopologyAttributesMatch(this, converter)) {
    return false;
  }

  return true;
}

}  // namespace blender::opensubdiv
