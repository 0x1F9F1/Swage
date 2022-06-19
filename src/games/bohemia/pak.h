#pragma once

namespace Swage
{
    class Stream;
    class FileDevice;
} // namespace Swage

namespace Swage::Bohemia
{
    Rc<FileDevice> LoadPAK(Rc<Stream> input);
} // namespace Swage::Bohemia
