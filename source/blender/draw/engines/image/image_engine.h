/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "DRW_render.hh"

namespace blender::image_engine {

struct Engine : public DrawEngine::Pointer {
  DrawEngine *create_instance() final;

  static void free_static();
};

}  // namespace blender::image_engine
