#include "glob.h"

#include "filedevice.h"

namespace Swage
{
    // Based on https://www.codeproject.com/Articles/5163931/Fast-String-Matching-with-Wildcards-Globs-and-Giti
    Tuple<bool, bool> Globber(StringView text, StringView glob, usize start)
    {
        usize i = start;
        usize j = start;

        usize const n = text.size();
        usize const m = glob.size();

        usize text1_backup = StringView::npos;
        usize glob1_backup = StringView::npos;
        usize text2_backup = StringView::npos;
        usize glob2_backup = StringView::npos;

        while (i < n)
        {
            if (j < m)
            {
                switch (glob[j])
                {
                    case '*':
                        if (++j < m && glob[j] == '*')
                        {
                            // trailing ** match everything
                            if (++j >= m)
                                return {true, true};

                            // ** followed by a / match zero or more directories
                            if (glob[j] == '/')
                                ++j;

                            // new **-loop, discard *-loop
                            text1_backup = StringView::npos;
                            glob1_backup = StringView::npos;
                            text2_backup = i;
                            glob2_backup = j;
                            continue;
                        }

                        if (text[i] == '/')
                            break;

                        // trailing * matches everything except /
                        text1_backup = i;
                        glob1_backup = j;
                        continue;
                    case '?':
                        // match any character except /
                        if (text[i] == '/')
                            break;
                        i++;
                        j++;
                        continue;
                    case '[': {
                        // match any character in [...] except /
                        if (text[i] == '/')
                            break;
                        int lastchr;
                        bool matched = false;
                        bool reverse = j + 1 < m && (glob[j + 1] == '^' || glob[j + 1] == '!');
                        // inverted character class
                        if (reverse)
                            j++;
                        // match character class
                        for (lastchr = 256; ++j < m && glob[j] != ']'; lastchr = ToLower(glob[j]))
                            if (lastchr < 256 && glob[j] == '-' && j + 1 < m
                                    ? ToLower(text[i]) <= ToLower(glob[++j]) && ToLower(text[i]) >= lastchr
                                    : ToLower(text[i]) == ToLower(glob[j]))
                                matched = true;
                        if (matched == reverse)
                            break;
                        i++;
                        if (j < m)
                            j++;
                        continue;
                    }
                    case '\\':
                        // literal match \-escaped character
                        if (j + 1 < m)
                            j++;
                        // FALLTHROUGH
                    default:
                        // match the current non-NUL character
                        if (ToLower(glob[j]) != ToLower(text[i]))
                            break;

                        i++;
                        j++;

                        continue;
                }
            }

            if (glob1_backup != StringView::npos)
            {
                // *-loop: backtrack to the last * but do not jump over /
                i = ++text1_backup;
                j = glob1_backup;

                if (text1_backup < n && text[text1_backup] == '/')
                {
                    text1_backup = StringView::npos;
                    glob1_backup = StringView::npos;
                }

                continue;
            }

            if (glob2_backup != StringView::npos)
            {
                // **-loop: backtrack to the last **

                // do not match in the middle of a file/folder when coming from a **/
                if (glob[glob2_backup - 1] == '/')
                    while (text2_backup < n && text[text2_backup] != '/')
                        ++text2_backup;

                i = ++text2_backup;
                j = glob2_backup;

                continue;
            }

            return {false, glob2_backup != StringView::npos};
        }

        // ignore trailing stars
        while (j < m && glob[j] == '*')
            j++;

        // at end of text means success if nothing else is left to match
        return {j >= m, (glob2_backup != StringView::npos) || (n == 0) || (j < m && glob[j] == '/')};
    }

    usize LiteralPrefix(StringView pattern)
    {
        usize i = 0, j = 0;

        while (i < pattern.size())
        {
            switch (pattern[i++])
            {
                case '*':
                case '?':
                case '[': return j;

                case '/': j = i; break;
            }
        }

        return j;
    }

    FileGlobber::FileGlobber(Rc<FileDevice> device, String path, String pattern)
        : device_(std::move(device))
        , base_(std::move(path))
        , pattern_(std::move(pattern))
    {
        start_ = LiteralPrefix(pattern_);
        pending_.emplace(pattern_.substr(0, start_));
    }

    bool FileGlobber::Next(FolderEntry& entry)
    {
        do
        {
            if (current_)
            {
                while (current_->Next(entry))
                {
                    entry.Name.insert(0, here_);

                    auto [matched, candidate] = Globber(entry.Name, pattern_, start_);

                    if (entry.IsFolder && candidate)
                        pending_.emplace(entry.Name + "/");

                    if (matched)
                        return true;
                }

                current_ = nullptr;
            }

            while (!pending_.empty())
            {
                here_ = std::move(pending_.front());

                pending_.pop();

                current_ = device_->Enumerate(base_ + here_);

                if (current_)
                    break;
            }
        } while (current_);

        return false;
    }
} // namespace Swage
