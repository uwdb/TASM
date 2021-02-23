#ifndef TASM_TRANSACTION_H
#define TASM_TRANSACTION_H

#include "Files.h"
#include "TileLayout.h"
#include "Video.h"
#include <mutex>
#include "LayoutDatabase.h"

class Transaction;
class OutputStream {
public:
    OutputStream(const Transaction &transaction,
                 const tasm::TiledEntry &entry,
                 unsigned int tileNumber,
                 unsigned int firstFrame,
                 unsigned int lastFrame)
            : transaction_(transaction),
            entry_(entry),
              filename_(tasm::TileFiles::temporaryTileFilename(entry, tileNumber, firstFrame, lastFrame)),
              codec_(Codec::HEVC),
              stream_(filename_)
    { }

    OutputStream(const OutputStream&) = delete;
    OutputStream(OutputStream&&) = default;

    std::ofstream& stream() { return stream_; }
    const std::experimental::filesystem::path &filename() const { return filename_; }
    const auto &codec() const { return codec_; }

protected:
    const Transaction &transaction_;
    const tasm::TiledEntry &entry_;
    const std::experimental::filesystem::path filename_;
    const Codec codec_;
    std::ofstream stream_;
};

class Transaction {
public:
    Transaction(const Transaction&) = delete;
    Transaction(Transaction&&) = default;

    virtual ~Transaction() = default;

    unsigned long id() const { return id_; }
    std::list<OutputStream> &outputs() { return outputs_; }
    const std::list<OutputStream> &outputs() const { return outputs_; }

    virtual void commit() = 0;
    virtual void abort() = 0;

//    virtual OutputStream& write(const tasm::Video &video, const std::string &name, Codec codec) {
//        return outputs_.emplace_back(*this, video, codec);
//    }
//    virtual OutputStream& write(const std::experimental::filesystem::path &path, const CompositeVolume &volume,
//                                const GeometryReference &geometry,const Codec &codec) {
//        return outputs_.emplace_back(*this, path, volume, geometry, codec);
//    }

protected:
    explicit Transaction(const unsigned long id)
            : id_(id)
    { }

    static std::recursive_mutex global;
    std::list<OutputStream> outputs_; // Write returns references, which container must not invalidate

private:
    unsigned long id_;
};

class TileCrackingTransaction: public Transaction {
public:
    TileCrackingTransaction(std::shared_ptr<tasm::TiledEntry> entry, const tasm::TileLayout &tileLayout, int firstFrame = -1, int lastFrame = -1)
            : Transaction(0u),
              entry_(entry),
              tileLayout_(tileLayout),
              firstFrame_(firstFrame),
              lastFrame_(lastFrame),
              complete_(false)
    {
        prepareTileDirectory();
    }

    ~TileCrackingTransaction() override {
        if (!complete_)
            commit();
    }

    virtual OutputStream& write(unsigned int tileNumber) {
        return outputs_.emplace_back(*this,
                                     *entry_,
                                     tileNumber,
                                     firstFrame_,
                                     lastFrame_);
    }

    void commit() override;

    void abort() override;

private:
    void prepareTileDirectory();
    void writeTileMetadata();

    std::shared_ptr<tasm::TiledEntry> entry_;
    const tasm::TileLayout tileLayout_;

    int firstFrame_;
    int lastFrame_;

    bool complete_;
};

#endif //TASM_TRANSACTION_H
