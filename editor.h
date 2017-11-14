// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include "buffer.h"
#include "render.h"
#include "theme.h"

#include <string>

struct AppEnv {
  std::string clipboard;
};

class Editor {
 public:
  Editor(Site* site) : site_(site) {}

  // state management
  void UpdateState(const EditNotification& state) { state_ = state; }
  const EditNotification& CurrentState() { return state_; }

  bool HasMostRecentEdit() {
    return state_.shutdown || check_most_recent_edit_(state_);
  }

  bool HasCommands() { return state_.shutdown || !commands_.empty(); }

  EditResponse MakeResponse() {
    EditResponse r;
    r.done = state_.shutdown;
    r.become_used = !commands_.empty();
    commands_.swap(r.content);
    assert(commands_.empty());
    return r;
  }

  // editor commands
  void SelectLeft();
  void MoveLeft();
  void SelectRight();
  void MoveRight();
  void MoveStartOfLine();
  void MoveEndOfLine();
  void MoveDownN(int n);
  void MoveUpN(int n);
  void MoveDown() { MoveDownN(1); }
  void MoveUp() { MoveUpN(1); }
  void SelectDownN(int n);
  void SelectUpN(int n);
  void SelectDown() { SelectDownN(1); }
  void SelectUp() { SelectUpN(1); }
  void Backspace();
  void Copy(AppEnv* env);
  void Cut(AppEnv* env);
  void Paste(AppEnv* env);
  void InsNewLine() { InsChar('\n'); }
  void InsChar(char c);

  // rendering
 private:
  struct CharInfo {
    char c;
    bool selected;
    bool cursor;
    Tag tag;
  };
  struct LineInfo {
    bool cursor;
  };

  typedef std::function<void(LineInfo, const std::vector<CharInfo>&)>
      LineCallback;

 public:
  template <class RC>
  void Render(RC* parent_context) {
    RenderThing(&Editor::RenderCommon, parent_context);
  }

  template <class RC>
  void RenderSideBar(RC* parent_context) {
    RenderThing(&Editor::RenderSideBarCommon, parent_context);
  }

 private:
  static uint32_t ThemeFlags(const LineInfo* li, const CharInfo* ci) {
    uint32_t f = 0;
    if (li->cursor) {
      f |= Theme::HIGHLIGHT_LINE;
    }
    if (ci && ci->selected) {
      f |= Theme::SELECTED;
    }
    return f;
  }

  void CursorLeft();
  void CursorRight();
  void CursorDown();
  void CursorUp();
  void CursorStartOfLine();
  void CursorEndOfLine();

  void SetSelectMode(bool sel);
  bool SelectMode() const { return selection_anchor_ != ID(); }
  void DeleteSelection();

  void NextRenderMustHaveID(ID id);
  void NextRenderMustNotHaveID(ID id);

  template <class RC>
  struct EditRenderContext {
    RC* parent_context;
    decltype(static_cast<RC*>(nullptr)->color) color;
    const typename Renderer<EditRenderContext>::Rect* window;
    int ofs_row;

    template <class T, class A>
    void Put(int row, int col, const T& val, const A& attr) {
      if (row < 0 || col < 0 || row >= window->height() ||
          col >= window->width()) {
        return;
      }
      parent_context->Put(row + ofs_row + window->row(), col + window->column(),
                          val, attr);
    }

    void Move(int row, int col) {
      Log() << "EDMOVE: " << row << ", " << col;
      if (row < 0 || col < 0 || row >= window->height() ||
          col >= window->width()) {
        return;
      }
      parent_context->Move(row + ofs_row + window->row(),
                           col + window->column());
    }
  };

  template <class RC>
  void RenderThing(void (Editor::*thing)(int height, LineCallback),
                   RC* parent_context) {
    Renderer<EditRenderContext<RC>> renderer;
    auto container = renderer.AddContainer(LAY_COLUMN)
                         .FixSize(parent_context->window->width(), 0);
    typename Renderer<EditRenderContext<RC>>::ItemRef cursor_row;
    (this->*thing)(
        parent_context->window->height(),
        [&container, &cursor_row, parent_context](
            LineInfo li, const std::vector<CharInfo>& ci) {
          auto ref =
              container
                  .AddItem(LAY_TOP | LAY_LEFT,
                           [li, ci](EditRenderContext<RC>* ctx) {
                             int col = 0;
                             for (const auto& c : ci) {
                               if (c.cursor) {
                                 ctx->Move(0, col);
                               }
                               ctx->Put(0, col++, c.c,
                                        ctx->color->Theme(c.tag,
                                                          ThemeFlags(&li, &c)));
                             }
                             while (col < ctx->window->width()) {
                               ctx->Put(0, col++, ' ',
                                        ctx->color->Theme(
                                            Tag(), ThemeFlags(&li, nullptr)));
                             }
                           })
                  .FixSize(parent_context->window->width(), 1);
          if (li.cursor) {
            cursor_row = ref;
          }
        });
    renderer.Layout();
    EditRenderContext<RC> ctx{
        parent_context, parent_context->color, nullptr,
        // out_row = buf_row + ofs
        // => cursor_row_ = cursor_row.Rect().row() + ofs
        // => ofs = cursor_row_ - cursor_row.Rect().row()
        cursor_row_ - (cursor_row ? cursor_row.GetRect().row() : 0)};
    renderer.Draw(&ctx);
  }

  Site* const site_;
  int cursor_row_ = 0;  // cursor row as an offset into the view buffer
  ID cursor_ = String::Begin();
  ID selection_anchor_ = ID();
  EditNotification state_;
  std::vector<String::CommandPtr> commands_;
  Tag cursor_token_;
  SideBufferRef active_side_buffer_;
  std::function<bool(const EditNotification&)> check_most_recent_edit_ =
      [](const EditNotification&) { return true; };

  void RenderCommon(int approx_height, LineCallback callback);
  void RenderSideBarCommon(int approx_height, LineCallback callback);
};
