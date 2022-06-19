#pragma once

namespace Swage
{
    class Stream;
    class FileDevice;
} // namespace Swage

namespace Swage
{
    Rc<FileDevice> LoadZip(Rc<Stream> input);
} // namespace Swage
