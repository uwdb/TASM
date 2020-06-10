#include "Transaction.h"

#include "Gpac.h"
#include <iostream>

void TileCrackingTransaction::prepareTileDirectory() {
    auto directory = tasm::TileFiles::directoryForTilesInFrames(*entry_, firstFrame_, lastFrame_);
    bool isRelative = directory.is_relative();
    std::error_code error;
    if (!std::filesystem::create_directory(directory, error))
        std::cerr << "Failed to create tile directory: " << error.message() << std::endl;
}

void TileCrackingTransaction::abort() {
    complete_ = true;
    for(const auto &output: outputs())
        std::filesystem::remove(output.filename());
}

void TileCrackingTransaction::commit() {
    complete_ = true;

    for (auto &output : outputs()) {
        output.stream().close();

        // Mux the outputs to mp4.
        auto muxedFile = output.filename();
        muxedFile.replace_extension(tasm::TileFiles::muxedFilenameExtension());
        tasm::gpac::mux_media(output.filename(), muxedFile);
    }

    writeTileMetadata();

    entry_->incrementTileVersion();
}

void TileCrackingTransaction::writeTileMetadata() {
    auto metadataFilename = tasm::TileFiles::tileMetadataFilename(*entry_, firstFrame_, lastFrame_);
    tasm::gpac::write_tile_configuration(metadataFilename, tileLayout_);
}