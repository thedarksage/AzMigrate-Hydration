
///
/// \file globtoregex.h
///
/// \brief converts glob to a regex
///

#ifndef GLOBTOREGEX_H
#define GLOBTOREGEX_H

/// \brief used to convert glob to reqular expresion
class Glob {
public:
    /// \brief converts a glob to a regular expression
    static void globToRegex(std::string const & glob,  ///< glob to convert to regex
                            std::string& regx          ///< receives the regex
                            )
    {
        std::string::size_type gLen = glob.size();
        // need to deal with both glob chars and any Perl regex chars that maybe in the name
        // note: not all the Perl regex chars are allowed in a filename, but to place it safe just
        // escape all regex chars that are not also glob chars
        for (std::string::size_type gIdx = 0; gIdx < gLen; ++gIdx) {
            switch(glob[gIdx]) {
                // note: all glob chars are cased (even though some could be handled
                // by the default case) to be obvious about glob vs regex
                case '.':
                    // globs treat '.' as '.' but regex treats '.' as "match
                    // any char", make sure regex treats '.' as match '.' only
                    regx += "\\.";
                    break;
                case '*':
                    if (gIdx == 0 || '*' != glob[gIdx - 1])
                        // either the first char was a '*' or
                        // the preceeding char was not a'*' so it is OK
                        // to replace it with the regex. there is no
                        // need to replace consecutive '*' each with a
                        // regex as the first one will do.
                        //
                        // e.g. if you have "***" you only need to replace the
                        // first '*' and just skip over the other consecutive ones
                        regx += "[[:print:]|[:cntrl:]]*";
                    ;
                    break;
                case '?':
                    // globs ? is match any char but and it must be there
                    // e.g. exp?ression would not match expression as it needs 1 char
                    // between exp and ression
                    regx += "[[:print:]|[:cntrl:]]?";
                    break;

                case '[':
                    // globs can use [ to indicate teh start of a char set
                    // regex does the same so keep [ as ]
                    regx +=  glob[gIdx];
                    break;
                case ']':
                    // globs use ] to indicate end of char set. since regex does  not
                    // treate ] as a special char (indicates end of a char set if
                    // there was a proceeding [ or just as ']' if no proceeding [)
                    // so keep ] as ]
                    regx +=  glob[gIdx];
                    break;

                    // remaing special regex chars that need to be escaped
                case '{':
                    regx += "\\{";
                    break;
                case '}':
                    regx += "\\}";
                    break;
                case '(':
                    regx += "\\(";
                    break;
                case ')':
                    regx += "\\)";
                    break;
                case '/':
                case '\\':
                    regx += "[\\\\/]";
                    break;
                case '+':
                    regx += "\\+";
                    break;
                case '|':
                    regx += "\\|";
                    break;
                case '^':
                    regx += "\\^";
                    break;
                case '$':
                    regx += "\\$";
                    break;

                    // non-glob -non regex char, use as is
                default:
                    regx += glob[gIdx];
                    break;
            }
        }
    }
};

#endif // GLOBTOREGEX_H
