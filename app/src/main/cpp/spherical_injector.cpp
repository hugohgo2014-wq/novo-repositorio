#include <jni.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>

namespace {
struct Box {
    uint64_t start = 0;
    uint64_t size = 0;
    uint64_t header = 0;
    std::string type;
    uint64_t end() const { return start + size; }
};

uint32_t be32(const std::vector<uint8_t>& b, uint64_t p) {
    return (uint32_t(b[p]) << 24) | (uint32_t(b[p+1]) << 16) |
           (uint32_t(b[p+2]) << 8) | uint32_t(b[p+3]);
}
uint64_t be64(const std::vector<uint8_t>& b, uint64_t p) {
    return (uint64_t(be32(b,p)) << 32) | be32(b,p+4);
}
void put32(std::vector<uint8_t>& b, uint64_t p, uint32_t v) {
    b[p] = uint8_t(v >> 24); b[p+1] = uint8_t(v >> 16);
    b[p+2] = uint8_t(v >> 8); b[p+3] = uint8_t(v);
}
void put64(std::vector<uint8_t>& b, uint64_t p, uint64_t v) {
    put32(b,p,uint32_t(v >> 32)); put32(b,p+4,uint32_t(v));
}

bool parseBox(const std::vector<uint8_t>& b, uint64_t pos, uint64_t end, Box& out) {
    if (pos + 8 > end || end > b.size()) return false;
    uint32_t s = be32(b, pos);
    out.start = pos;
    out.header = 8;
    out.type.assign(reinterpret_cast<const char*>(&b[pos+4]), 4);
    if (s == 1) {
        if (pos + 16 > end) return false;
        out.size = be64(b, pos+8);
        out.header = 16;
    } else if (s == 0) {
        out.size = end - pos;
    } else {
        out.size = s;
    }
    return out.size >= out.header && pos + out.size <= end;
}

void writeBoxSize(std::vector<uint8_t>& b, const Box& box, uint64_t newSize) {
    uint32_t old = be32(b, box.start);
    if (old == 1) put64(b, box.start + 8, newSize);
    else if (newSize <= 0xffffffffULL) put32(b, box.start, uint32_t(newSize));
}

bool isVideoTrack(const std::vector<uint8_t>& b, const Box& trak) {
    uint64_t p = trak.start + trak.header;
    while (p < trak.end()) {
        Box child;
        if (!parseBox(b, p, trak.end(), child)) return false;
        if (child.type == "mdia") {
            uint64_t q = child.start + child.header;
            while (q < child.end()) {
                Box md;
                if (!parseBox(b, q, child.end(), md)) return false;
                if (md.type == "hdlr" && md.size >= md.header + 12) {
                    uint64_t handler = md.start + md.header + 8;
                    return std::memcmp(&b[handler], "vide", 4) == 0;
                }
                q = md.end();
            }
        }
        p = child.end();
    }
    return false;
}

bool containsOurUuid(const std::vector<uint8_t>& b, const Box& trak) {
    static const uint8_t uuid[16] = {
        0xff,0xcc,0x82,0x63,0xf8,0x55,0x4a,0x93,
        0x88,0x14,0x58,0x7a,0x02,0x52,0x1f,0xdd
    };
    uint64_t p = trak.start + trak.header;
    while (p < trak.end()) {
        Box child;
        if (!parseBox(b, p, trak.end(), child)) return false;
        if (child.type == "uuid" && child.size >= child.header + 16 &&
            std::memcmp(&b[child.start + child.header], uuid, 16) == 0) return true;
        p = child.end();
    }
    return false;
}

bool isContainer(const std::string& t) {
    return t == "moov" || t == "trak" || t == "mdia" || t == "minf" ||
           t == "stbl" || t == "edts" || t == "dinf" || t == "udta" ||
           t == "mvex" || t == "moof" || t == "traf" || t == "mfra" ||
           t == "meta";
}

bool patchOffsets(std::vector<uint8_t>& b, uint64_t begin, uint64_t end, uint64_t delta) {
    uint64_t p = begin;
    while (p < end) {
        Box box;
        if (!parseBox(b, p, end, box)) return false;
        if (box.type == "stco" || box.type == "co64") {
            uint64_t payload = box.start + box.header;
            if (payload + 8 > box.end()) return false;
            uint32_t count = be32(b, payload + 4);
            uint64_t entry = payload + 8;
            uint64_t width = box.type == "stco" ? 4 : 8;
            if (entry + uint64_t(count) * width > box.end()) return false;
            for (uint32_t i = 0; i < count; ++i) {
                uint64_t old = width == 4 ? be32(b, entry + uint64_t(i)*width)
                                          : be64(b, entry + uint64_t(i)*width);
                if (old > std::numeric_limits<uint64_t>::max() - delta) return false;
                uint64_t value = old + delta;
                if (width == 4) {
                    if (value > 0xffffffffULL) return false;
                    put32(b, entry + uint64_t(i)*width, uint32_t(value));
                } else {
                    put64(b, entry + uint64_t(i)*width, value);
                }
            }
        } else if (isContainer(box.type)) {
            uint64_t childStart = box.start + box.header + (box.type == "meta" ? 4 : 0);
            if (childStart < box.end() && !patchOffsets(b, childStart, box.end(), delta)) return false;
        }
        p = box.end();
    }
    return true;
}

bool readAt(std::ifstream& in, uint64_t pos, uint8_t* dst, size_t count) {
    in.seekg(static_cast<std::streamoff>(pos), std::ios::beg);
    in.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(count));
    return in.good() || (in.eof() && size_t(in.gcount()) == count);
}

bool copyRange(std::ifstream& in, std::ofstream& out, uint64_t start, uint64_t length) {
    std::vector<char> buffer(1024 * 1024);
    uint64_t pos = start;
    while (length) {
        size_t chunk = size_t(std::min<uint64_t>(length, buffer.size()));
        in.clear();
        in.seekg(static_cast<std::streamoff>(pos), std::ios::beg);
        in.read(buffer.data(), static_cast<std::streamsize>(chunk));
        if (size_t(in.gcount()) != chunk) return false;
        out.write(buffer.data(), static_cast<std::streamsize>(chunk));
        if (!out.good()) return false;
        pos += chunk;
        length -= chunk;
    }
    return true;
}

std::string makeXml(int mode) {
    std::string xml =
        "<?xml version=\"1.0\"?>"
        "<rdf:SphericalVideo xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" "
        "xmlns:GSpherical=\"http://ns.google.com/videos/1.0/spherical/\">"
        "<GSpherical:Spherical>true</GSpherical:Spherical>"
        "<GSpherical:Stitched>true</GSpherical:Stitched>"
        "<GSpherical:StitchingSoftware>NEBULA 360 Studio</GSpherical:StitchingSoftware>"
        "<GSpherical:ProjectionType>equirectangular</GSpherical:ProjectionType>";
    if (mode == 1) xml += "<GSpherical:StereoMode>top-bottom</GSpherical:StereoMode>";
    if (mode == 2) xml += "<GSpherical:StereoMode>left-right</GSpherical:StereoMode>";
    xml += "</rdf:SphericalVideo>";
    return xml;
}

std::string inject(const std::string& inputPath, const std::string& outputPath, int mode) {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) return "Não foi possível abrir o vídeo de origem.";
    in.seekg(0, std::ios::end);
    uint64_t fileSize = uint64_t(in.tellg());
    if (fileSize < 16) return "Arquivo MP4 inválido ou muito pequeno.";

    uint64_t moovPos = UINT64_MAX, moovSize = 0, mdatPos = UINT64_MAX;
    uint64_t pos = 0;
    std::vector<uint8_t> header(16);
    while (pos + 8 <= fileSize) {
        if (!readAt(in, pos, header.data(), 8)) break;
        uint32_t size32 = be32(header, 0);
        uint64_t size = size32;
        uint64_t hsize = 8;
        if (size32 == 1) {
            if (!readAt(in, pos, header.data(), 16)) return "Cabeçalho MP4 estendido inválido.";
            size = be64(header, 8); hsize = 16;
        } else if (size32 == 0) {
            size = fileSize - pos;
        }
        if (size < hsize || pos + size > fileSize) return "Estrutura MP4 inválida.";
        std::string type(reinterpret_cast<char*>(header.data()+4), 4);
        if (type == "moov") { moovPos = pos; moovSize = size; }
        if (type == "mdat" && mdatPos == UINT64_MAX) mdatPos = pos;
        pos += size;
    }
    if (moovPos == UINT64_MAX) return "O arquivo não contém o bloco moov do MP4.";

    if (moovSize > 128ULL * 1024ULL * 1024ULL) return "O índice do MP4 (moov) é grande demais para processar.";
    std::vector<uint8_t> moov(static_cast<size_t>(moovSize), uint8_t{0});
    if (!readAt(in, moovPos, moov.data(), moov.size())) return "Falha ao ler o índice do MP4.";

    Box root;
    if (!parseBox(moov, 0, moov.size(), root) || root.type != "moov")
        return "Bloco moov inválido.";

    Box target;
    bool found = false;
    uint64_t p = root.start + root.header;
    while (p < root.end()) {
        Box child;
        if (!parseBox(moov, p, root.end(), child)) return "Estrutura interna do MP4 inválida.";
        if (child.type == "trak" && isVideoTrack(moov, child)) {
            target = child;
            found = true;
            break;
        }
        p = child.end();
    }
    if (!found) return "Nenhuma trilha de vídeo foi encontrada no MP4.";
    if (containsOurUuid(moov, target)) return "Este MP4 já possui metadados esféricos NEBULA.";

    const std::string xml = makeXml(mode);
    static const uint8_t uuid[16] = {
        0xff,0xcc,0x82,0x63,0xf8,0x55,0x4a,0x93,
        0x88,0x14,0x58,0x7a,0x02,0x52,0x1f,0xdd
    };
    uint64_t boxSize = 8 + 16 + xml.size();
    if (boxSize > 0xffffffffULL) return "Metadados excedem o limite do MP4.";
    std::vector<uint8_t> atom(size_t(boxSize), 0);
    put32(atom, 0, uint32_t(boxSize));
    std::memcpy(atom.data()+4, "uuid", 4);
    std::memcpy(atom.data()+8, uuid, 16);
    std::memcpy(atom.data()+24, xml.data(), xml.size());

    uint64_t insertion = target.end();
    moov.insert(moov.begin() + static_cast<std::ptrdiff_t>(insertion), atom.begin(), atom.end());
    const uint64_t delta = boxSize;
    writeBoxSize(moov, target, target.size + delta);
    Box updatedRoot;
    if (!parseBox(moov, 0, moov.size(), updatedRoot)) return "Falha ao atualizar o tamanho do moov.";
    writeBoxSize(moov, updatedRoot, moovSize + delta);

    // When moov precedes mdat, all absolute chunk offsets move by the inserted atom size.
    if (mdatPos > moovPos && !patchOffsets(moov, updatedRoot.start + updatedRoot.header,
                                           moov.size(), delta))
        return "Não foi possível atualizar os offsets dos quadros MP4.";

    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) return "Não foi possível criar o MP4 de saída.";
    if (!copyRange(in, out, 0, moovPos)) return "Falha ao copiar o início do MP4.";
    out.write(reinterpret_cast<const char*>(moov.data()), static_cast<std::streamsize>(moov.size()));
    if (!out.good()) return "Falha ao gravar os metadados esféricos.";
    uint64_t afterMoov = moovPos + moovSize;
    if (!copyRange(in, out, afterMoov, fileSize - afterMoov)) return "Falha ao copiar os dados de vídeo.";
    out.flush();
    if (!out.good()) return "Falha ao finalizar o MP4.";
    return "OK";
}
} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_nebula360_studio_NativeInjector_inject(JNIEnv* env, jclass,
                                                 jstring input, jstring output, jint mode) {
    const char* inChars = env->GetStringUTFChars(input, nullptr);
    const char* outChars = env->GetStringUTFChars(output, nullptr);
    if (!inChars || !outChars) return env->NewStringUTF("Falha ao acessar os caminhos dos arquivos.");
    std::string result = inject(inChars, outChars, int(mode));
    env->ReleaseStringUTFChars(input, inChars);
    env->ReleaseStringUTFChars(output, outChars);
    return env->NewStringUTF(result.c_str());
}
