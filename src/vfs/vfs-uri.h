/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#pragma once

#include <vector>

#include "vfs.h"

/*
   if uri - relative, then path is taken relative to path, and taken first FS from list
      if no FS, then this is error and returns empty clPtr<FS>

   path always must be absolute

   returns: clPtr<FS> and changed path
      in case of error: clPtr<FS> empty, path - undefined
*/

clPtr<FS> ParzeURI( const unicode_t* uri, FSPath& path, const std::vector<clPtr<FS>>& checkFS );

clPtr<FS> ParzeCurrentSystemURL( FSPath& path );
