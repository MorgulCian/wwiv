/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                Copyright (C)2020, WWIV Software Services               */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/**************************************************************************/
#include "sdk/files/files.h"

#include "core/datafile.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include "sdk/vardec.h"
#include <algorithm>
#include <string>
#include <utility>

using std::endl;
using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk::files {


std::string align(const std::string& file_name) {
  auto f{file_name};
  align(&f);
  return f;
}

void align(std::string *file_name) {
  char s[MAX_PATH];
  to_char_array(s, *file_name);
  _align(s);
  file_name->assign(s);
}

void _align(char* fn) {
  // TODO Modify this to handle long filenames
  char name[40], ext[40];

  bool invalid = false;
  if (fn[0] == '.') {
    invalid = true;
  }

  for (size_t i = 0; i < size(fn); i++) {
    if (fn[i] == '\\' || fn[i] == '/' || fn[i] == ':' || fn[i] == '<' || fn[i] == '>' ||
        fn[i] == '|') {
      invalid = true;
    }
  }
  if (invalid) {
    strcpy(fn, "        .   ");
    return;
  }
  char* s2 = strrchr(fn, '.');
  if (s2 == nullptr || strrchr(fn, '\\') > s2) {
    ext[0] = '\0';
  } else {
    strcpy(ext, &(s2[1]));
    ext[3] = '\0';
    s2[0] = '\0';
  }
  strcpy(name, fn);

  for (int j = strlen(name); j < 8; j++) {
    name[j] = ' ';
  }
  name[8] = '\0';
  auto has_wildcard = false;
  auto has_space = false;
  for (auto k = 0; k < 8; k++) {
    if (name[k] == '*') {
      has_wildcard = true;
    }
    if (name[k] == ' ') {
      has_space = true;
    }
    if (has_space) {
      name[k] = ' ';
    }
    if (has_wildcard) {
      name[k] = '?';
    }
  }

  for (int i2 = strlen(ext); i2 < 3; i2++) {
    ext[i2] = ' ';
  }
  ext[3] = '\0';
  has_wildcard = false;
  for (int i3 = 0; i3 < 3; i3++) {
    if (ext[i3] == '*') {
      has_wildcard = true;
    }
    if (has_wildcard) {
      ext[i3] = '?';
    }
  }

  char buffer[MAX_PATH];
  for (int i4 = 0; i4 < 12; i4++) {
    buffer[i4] = SPACE;
  }
  strcpy(buffer, name);
  buffer[8] = '.';
  strcpy(&(buffer[9]), ext);
  strcpy(fn, buffer);
  for (int i5 = 0; i5 < 12; i5++) {
    fn[i5] = to_upper_case<char>(fn[i5]);
  }
}

static char *unalign_char(char *file_name) {
  char* temp = strstr(file_name, " ");
  if (temp) {
    *temp++ = '\0';
    char* temp2 = strstr(temp, ".");
    if (temp2) {
      strcat(file_name, temp2);
    }
  }
  return file_name;
}

std::string unalign(const std::string& file_name) { 
  char s[1025];
  to_char_array(s, file_name);
  const auto r = unalign_char(s);
  return ToStringLowerCase(r);
}


FileApi::FileApi(std::string data_directory)
: data_directory_(std::move(data_directory)) {
  clock_ = std::make_unique<SystemClock>();
};

bool FileApi::Exist(const std::string& filename) const {
  return File::Exists(PathFilePath(data_directory_, StrCat(filename, ".dir")));
}

bool FileApi::Exist(const directoryrec& dir) const {
  return Exist(dir.filename);
}

bool FileApi::Create(const std::string& filename) {
  if (Exist(filename)) {
    return false;
  }

  FileArea area(this, data_directory_, filename);
  return area.FixFileHeader();
}

bool FileApi::Create(const directoryrec& dir) {
  return Create(dir.filename);  
}


bool FileApi::Remove(const std::string& filename) {
  // TODO(rushfan): Implement me.
  return false;
}

std::unique_ptr<FileArea> FileApi::Open(const std::string& filename) {
  if (!Exist(filename)) {
    return {};
  }
  return std::make_unique<FileArea>(this, data_directory_, filename);
}

std::unique_ptr<FileArea> FileApi::Open(const directoryrec& dir) {
  return Open(dir.filename);
}

const Clock* FileApi::clock() const noexcept {
  return clock_.get();  
}

void FileApi::set_clock(std::unique_ptr<Clock> clock) {
  clock_ = std::move(clock);
}


bool FileRecord::set_filename(const std::string unaligned_filename) {
  if (unaligned_filename.size() > 12) {
    return false;
  }
  auto aligned = align(unaligned_filename);
  to_char_array(u_.filename, aligned);
  return true;
}

std::string FileRecord::aligned_filename() {
  return align(u_.filename);
}

std::string FileRecord::unaligned_filename() {
  return unalign(u_.filename);
}

// Delegates to other constructor
FileArea::FileArea(FileApi* api, std::string data_directory, const directoryrec& dir)
    : FileArea(api, std::move(data_directory), dir.filename) {
  dir_ = dir; 
}

// Real constructor that does all of the work.
FileArea::FileArea(FileApi* api, std::string data_directory, const std::string& filename)
    : api_(api), data_directory_(std::move(data_directory)), filename_(StrCat(filename, ".dir")) {
  DataFile<uploadsrec> file(path(), File::modeReadOnly | File::modeBinary);
  if (!file) {
    return;
  }

  if (file.ReadVector(files_)) {
    open_ = true;
  }
}


// ReSharper disable once CppMemberFunctionMayBeConst
bool FileArea::FixFileHeader() {
  DataFile<uploadsrec> file(path(),File::modeReadWrite | File::modeCreateFile | File::modeBinary);
  uploadsrec h{};

  if (file.number_of_records() > 0) {
    file.Read(0, &h);
  }
  // Always overwrite marker.
  to_char_array(h.filename, "|MARKER|");
  if (h.daten == 0) {
    const auto now = api_->clock()->Now();
    h.daten = now.to_daten_t();
  }
  if (h.numbytes != file.number_of_records() - 1) {
    if (!file.Write(0, &h)) {
      return false;
    }
  }
  return true;
}

bool FileArea::Close() {
  if (open_ && dirty_) {
    return Save();
  }
  return true;
}

bool FileArea::Lock() { return false; }
bool FileArea::Unlock() { return false; }

int FileArea::number_of_files() const {
  if (!open_) {
    return 0;
  }

  if (files_.empty()) {
    return 0;
  }

  return wwiv::stl::size_int32(files_) - 1;
}

// File specific
FileRecord FileArea::ReadFile(int num) {
  return FileRecord(files_.at(num));
}

bool FileArea::AddFile(FileRecord& f) {
  files_.push_back(f.u());
  dirty_ = true;
  return true;
}

bool FileArea::DeleteFile(int file_number) {
  if (!wwiv::stl::erase_at(files_, file_number)) {
    return false;
  }
  dirty_ = true;
  return true;
}


bool FileArea::Save() {
  DataFile<uploadsrec> file(path(), File::modeReadWrite | File::modeCreateFile | File::modeBinary);

  if (!file) {
    return false;
  }

  const auto result = file.WriteVectorAndTruncate(files_);
  if (result) {
    dirty_ = false;
  }
  return result;
}

std::filesystem::path FileArea::path() const noexcept {
  return PathFilePath(data_directory_, filename_);
}


} // namespace wwiv::sdk::files