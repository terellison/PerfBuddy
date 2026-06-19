#include "pb_binary/analyzer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>

#include "pb_core/util.hpp"

namespace pb::binary {

namespace {

using Bytes = std::string;  // raw file contents

// Little-endian integer reads with bounds checking (return 0 if out of range).
uint64_t rd(const Bytes& b, size_t off, int n) {
  if (off + static_cast<size_t>(n) > b.size()) return 0;
  uint64_t v = 0;
  for (int i = 0; i < n; ++i)
    v |= static_cast<uint64_t>(static_cast<unsigned char>(b[off + i])) << (8 * i);
  return v;
}

struct Section {
  std::string name;
  uint64_t size = 0;
};

struct Parsed {
  std::string format = "unknown";
  std::string arch;
  std::vector<Section> sections;
  bool has_symbols = false;          // a symbol table is present (not stripped)
  uint64_t debug_bytes = 0;          // total size of .debug* / DWARF sections
  std::vector<std::pair<std::string, uint64_t>> top_symbols;  // name,size
};

// ---- ELF -------------------------------------------------------------------
Parsed parse_elf(const Bytes& b) {
  Parsed p;
  p.format = "ELF";
  bool is64 = (static_cast<unsigned char>(b[4]) == 2);
  p.arch = is64 ? "64-bit" : "32-bit";
  if (!is64) return p;  // size analysis works fine; section walk below is 64-bit

  uint64_t e_shoff = rd(b, 0x28, 8);
  uint16_t e_shentsize = static_cast<uint16_t>(rd(b, 0x3A, 2));
  uint16_t e_shnum = static_cast<uint16_t>(rd(b, 0x3C, 2));
  uint16_t e_shstrndx = static_cast<uint16_t>(rd(b, 0x3E, 2));
  if (e_shoff == 0 || e_shentsize < 64 || e_shnum == 0) return p;

  // Locate section-header string table.
  uint64_t shstr_off = 0, shstr_size = 0;
  {
    size_t sh = e_shoff + static_cast<size_t>(e_shstrndx) * e_shentsize;
    shstr_off = rd(b, sh + 24, 8);
    shstr_size = rd(b, sh + 32, 8);
  }
  auto name_at = [&](uint32_t nameoff) -> std::string {
    size_t at = shstr_off + nameoff;
    if (at >= b.size() || nameoff >= shstr_size) return {};
    std::string s;
    while (at < b.size() && b[at] != '\0') s += b[at++];
    return s;
  };

  uint64_t symtab_off = 0, symtab_size = 0, symtab_entsize = 0;
  uint64_t strtab_off = 0, strtab_size = 0;
  for (uint16_t i = 0; i < e_shnum; ++i) {
    size_t sh = e_shoff + static_cast<size_t>(i) * e_shentsize;
    uint32_t sh_name = static_cast<uint32_t>(rd(b, sh, 4));
    uint32_t sh_type = static_cast<uint32_t>(rd(b, sh + 4, 4));
    uint64_t sh_size = rd(b, sh + 32, 8);
    uint64_t sh_offset = rd(b, sh + 24, 8);
    std::string nm = name_at(sh_name);
    if (sh_type == 8 /*SHT_NOBITS .bss*/) {
      // .bss occupies no file space; record but it doesn't inflate the file.
    }
    p.sections.push_back({nm.empty() ? "<unnamed>" : nm, sh_size});
    if (nm.rfind(".debug", 0) == 0 || nm == ".zdebug_info") p.debug_bytes += sh_size;
    if (sh_type == 2 /*SHT_SYMTAB*/) {
      symtab_off = sh_offset;
      symtab_size = sh_size;
      symtab_entsize = rd(b, sh + 56, 8);
      p.has_symbols = true;
    }
    if (nm == ".strtab") { strtab_off = sh_offset; strtab_size = sh_size; }
  }

  // Heaviest symbols from .symtab/.strtab (Elf64_Sym is 24 bytes).
  if (symtab_off && symtab_entsize >= 24 && strtab_off) {
    uint64_t count = symtab_size / symtab_entsize;
    std::vector<std::pair<std::string, uint64_t>> syms;
    for (uint64_t i = 0; i < count; ++i) {
      size_t e = symtab_off + i * symtab_entsize;
      uint32_t st_name = static_cast<uint32_t>(rd(b, e, 4));
      uint64_t st_size = rd(b, e + 16, 8);
      if (st_size == 0) continue;
      size_t at = strtab_off + st_name;
      if (at >= b.size() || st_name >= strtab_size) continue;
      std::string nm;
      while (at < b.size() && b[at] != '\0') nm += b[at++];
      if (!nm.empty()) syms.emplace_back(nm, st_size);
    }
    std::sort(syms.begin(), syms.end(),
              [](auto& a, auto& c) { return a.second > c.second; });
    if (syms.size() > 10) syms.resize(10);
    p.top_symbols = std::move(syms);
  }
  return p;
}

// ---- PE --------------------------------------------------------------------
Parsed parse_pe(const Bytes& b) {
  Parsed p;
  p.format = "PE";
  uint32_t e_lfanew = static_cast<uint32_t>(rd(b, 0x3C, 4));
  if (b.compare(e_lfanew, 4, std::string("PE\0\0", 4)) != 0) return p;
  size_t coff = e_lfanew + 4;
  uint16_t machine = static_cast<uint16_t>(rd(b, coff, 2));
  p.arch = (machine == 0x8664) ? "x64" : (machine == 0x14c ? "x86" : "other");
  uint16_t nsections = static_cast<uint16_t>(rd(b, coff + 2, 2));
  uint32_t nsymbols = static_cast<uint32_t>(rd(b, coff + 12, 4));
  uint16_t opt_size = static_cast<uint16_t>(rd(b, coff + 16, 2));
  p.has_symbols = (nsymbols > 0);
  size_t sect = coff + 20 + opt_size;
  for (uint16_t i = 0; i < nsections; ++i) {
    size_t s = sect + static_cast<size_t>(i) * 40;
    if (s + 40 > b.size()) break;
    std::string nm(b.data() + s, 8);
    nm.erase(nm.find_last_not_of('\0') + 1);
    uint64_t raw = rd(b, s + 16, 4);  // SizeOfRawData
    p.sections.push_back({nm.empty() ? "<unnamed>" : nm, raw});
    if (nm.rfind(".debug", 0) == 0) p.debug_bytes += raw;
  }
  return p;
}

// ---- Mach-O ----------------------------------------------------------------
Parsed parse_macho(const Bytes& b, bool is64) {
  Parsed p;
  p.format = "Mach-O";
  p.arch = is64 ? "64-bit" : "32-bit";
  if (!is64) return p;
  uint32_t ncmds = static_cast<uint32_t>(rd(b, 16, 4));
  size_t off = 32;  // after mach_header_64
  for (uint32_t i = 0; i < ncmds; ++i) {
    uint32_t cmd = static_cast<uint32_t>(rd(b, off, 4));
    uint32_t cmdsize = static_cast<uint32_t>(rd(b, off + 4, 4));
    if (cmdsize == 0) break;
    if (cmd == 0x19 /*LC_SEGMENT_64*/) {
      std::string segname(b.data() + off + 8, 16);
      segname.erase(segname.find_last_not_of('\0') + 1);
      uint64_t filesize = rd(b, off + 8 + 16 + 8 + 8 + 8, 8);  // after segname,vmaddr,vmsize,fileoff
      p.sections.push_back({segname.empty() ? "<seg>" : segname, filesize});
      if (segname.find("DWARF") != std::string::npos) p.debug_bytes += filesize;
    }
    if (cmd == 0x2 /*LC_SYMTAB*/) p.has_symbols = true;
    off += cmdsize;
  }
  return p;
}

Parsed parse_any(const Bytes& b) {
  if (b.size() >= 4 && static_cast<unsigned char>(b[0]) == 0x7F && b[1] == 'E' &&
      b[2] == 'L' && b[3] == 'F')
    return parse_elf(b);
  if (b.size() >= 2 && b[0] == 'M' && b[1] == 'Z') return parse_pe(b);
  uint32_t magic = static_cast<uint32_t>(rd(b, 0, 4));
  if (magic == 0xFEEDFACF) return parse_macho(b, true);
  if (magic == 0xFEEDFACE) return parse_macho(b, false);
  if (magic == 0xCFFAEDFE || magic == 0xCEFAEDFE) return parse_macho(b, true);
  Parsed p;  // unknown
  return p;
}

}  // namespace

bool BinaryAnalyzer::supports(const Target& t) const {
  return t.executable && util::path_exists(*t.executable);
}

ModuleReport BinaryAnalyzer::analyze(const Target& t) const {
  auto start = std::chrono::steady_clock::now();
  ModuleReport report;
  report.module = name();
  report.version = "0.1.0";
  report.target_label = t.label;

  auto data = util::read_file(*t.executable);
  if (!data) {
    report.error = "could not read executable: " + *t.executable;
    return report;
  }
  const uint64_t total = data->size();
  Parsed p = parse_any(*data);

  report.summary["file_bytes"] = static_cast<double>(total);
  report.summary["debug_bytes"] = static_cast<double>(p.debug_bytes);
  report.summary["section_count"] = static_cast<double>(p.sections.size());

  auto add = [&](Finding f) { report.findings.push_back(std::move(f)); };

  if (p.format == "unknown") {
    Finding f;
    f.id = "binary.unknown_format";
    f.severity = Severity::Info;
    f.category = Category::BinarySize;
    f.title = "Unrecognized executable format (size = " + util::human_bytes(total) + ")";
    f.description = "File is not ELF, PE, or Mach-O; only total size is reported.";
    f.impact = "Detailed section/symbol analysis unavailable for this format.";
    f.remediation = "Provide an ELF, PE, or Mach-O game binary for full analysis.";
    f.metrics["file_bytes"] = static_cast<double>(total);
    add(std::move(f));
    report.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - start).count();
    return report;
  }

  // Headline: overall size.
  {
    Finding f;
    f.id = "binary.overview";
    f.severity = Severity::Info;
    f.category = Category::BinarySize;
    f.title = p.format + " " + p.arch + " binary, " + util::human_bytes(total);
    f.description = std::to_string(p.sections.size()) + " sections/segments parsed.";
    f.impact = "Baseline for download size, load time, and memory footprint.";
    f.remediation = "Use the findings below to identify what to trim.";
    f.metrics["file_bytes"] = static_cast<double>(total);
    add(std::move(f));
  }

  // Largest sections.
  auto secs = p.sections;
  std::sort(secs.begin(), secs.end(),
            [](const Section& a, const Section& c) { return a.size > c.size; });
  int shown = 0;
  for (const auto& s : secs) {
    if (s.size == 0) continue;
    if (shown++ >= 6) break;
    double share = total ? static_cast<double>(s.size) / total : 0.0;
    Finding f;
    f.id = "binary.section";
    f.category = Category::BinarySize;
    f.severity = share > 0.40 ? Severity::Medium
                 : share > 0.20 ? Severity::Low
                                : Severity::Info;
    f.title = "Section " + s.name + " = " + util::human_bytes(s.size) +
              " (" + std::to_string(static_cast<int>(share * 100)) + "% of file)";
    Location loc; loc.path = util::filename_of(*t.executable); loc.symbol = s.name;
    f.location = loc;
    f.impact = "Large contributor to executable size.";
    if (s.name == ".text")
      f.remediation = "Enable -O2/LTO, dead-code elimination (-ffunction-sections "
                      "-Wl,--gc-sections), and trim template bloat.";
    else if (s.name.rfind(".debug", 0) == 0 || s.name.find("DWARF") != std::string::npos)
      f.remediation = "Ship a stripped binary; keep debug info in a separate symbol file.";
    else if (s.name == ".rodata" || s.name == ".rdata")
      f.remediation = "Compress or externalize embedded read-only data/assets.";
    else
      f.remediation = "Review whether this section can be reduced or externalized.";
    f.metrics["bytes"] = static_cast<double>(s.size);
    f.metrics["share"] = share;
    add(std::move(f));
  }

  // Debug info weight.
  if (p.debug_bytes > 0) {
    double share = total ? static_cast<double>(p.debug_bytes) / total : 0.0;
    Finding f;
    f.id = "binary.debug_info";
    f.category = Category::BinarySize;
    f.severity = share > 0.25 ? Severity::High
                 : share > 0.10 ? Severity::Medium
                                : Severity::Low;
    f.title = "Debug info is " + util::human_bytes(p.debug_bytes) + " (" +
              std::to_string(static_cast<int>(share * 100)) + "% of file)";
    f.description = "DWARF/.debug sections are present in the shipped binary.";
    f.impact = "Inflates download size and load time with no runtime benefit.";
    f.remediation = "Strip for release (strip / -s / objcopy --only-keep-debug) "
                    "and distribute symbols separately.";
    f.metrics["debug_bytes"] = static_cast<double>(p.debug_bytes);
    add(std::move(f));
  }

  // Stripped or not.
  if (p.has_symbols && p.debug_bytes == 0) {
    Finding f;
    f.id = "binary.not_stripped";
    f.category = Category::BinarySize;
    f.severity = Severity::Low;
    f.title = "Binary is not fully stripped (symbol table present)";
    f.impact = "Symbol tables add size and expose internal symbol names.";
    f.remediation = "Run `strip` on the release build to drop the symbol table.";
    add(std::move(f));
  }

  // Heaviest symbols (ELF).
  int sc = 0;
  for (const auto& sym : p.top_symbols) {
    if (sc++ >= 5) break;
    Finding f;
    f.id = "binary.large_symbol";
    f.category = Category::BinarySize;
    f.severity = sym.second > 65536 ? Severity::Low : Severity::Info;
    std::string nm = sym.first.size() > 80 ? sym.first.substr(0, 77) + "..." : sym.first;
    f.title = "Large symbol " + nm + " = " + util::human_bytes(sym.second);
    Location loc; loc.symbol = sym.first; f.location = loc;
    f.impact = "Individual function/object contributing notable size.";
    f.remediation = "Check for template instantiation bloat, large static tables, "
                    "or inlining that could be reduced.";
    f.metrics["bytes"] = static_cast<double>(sym.second);
    add(std::move(f));
  }

  report.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start).count();
  return report;
}

}  // namespace pb::binary
