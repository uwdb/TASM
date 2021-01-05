

#ifndef LIGHTDB_TILES_OPAQUE_H
#define LIGHTDB_TILES_OPAQUE_H

#include <string>
#include "Nal.h"

namespace lightdb {

  class Opaque : public Nal {
  public:

      Opaque(const tiles::Context &context, const bytestring &data);

      /**
       *
       * @return A string wtih the bytes of this Nal
       */
      bytestring GetBytes() const override;

  };

}

#endif //LIGHTDB_TILES_OPAQUE_H
