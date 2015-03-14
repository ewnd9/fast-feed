#include <node.h>
#include <nan.h>
#include <v8.h>
#include <sstream>
#include <string.h>
#include "rapidxml.hpp"

// Used example code from:
// https://github.com/glynos/cpp-netlib/blob/master/contrib/http_examples/rss/rss.cpp
// http://nodejs.org/api/addons.html

using namespace v8;
using namespace rapidxml;

char const *EMPTY_C_STRING = "";

// Helper to read text node value.
// Returns 0 when cannot read the value.

char const *readTextNode(xml_node<char> *rootNode, const char* name) {

    xml_node<char> *node = rootNode->first_node(name);

    if (node) {

        xml_node<char> *textNode = node->first_node();

        if (textNode) {

            return textNode->value();

        } else {

            return EMPTY_C_STRING;
        }

    } else {

        return 0;
    }
}

// Helper to find the line number of error.

std::pair<int, int> findErrorLine(const char* xml, const char* where) {

    int i = 0;
    int ln = 1;
    char ch = 0;
    int col = 1;

    while (true) {

        if (xml + i == where) {

            break;
        }

        ch = xml[i];

        if (ch == '\n') {

            ln++;

            col = 0;

        } else {

            col++;
        }

        i++;
    }

    return std::pair<int, int>(ln, col);
}

// Parses the Atom feed.

void parseAtomFeed(xml_node<char> *feedNode, const Local<Object> &feed, bool extractContent) {

    feed->Set(NanNew<String>("type"), NanNew<String>("atom"));

    // Extracts the title property.

    char const *title = readTextNode(feedNode, "title");

    if (title) {

        feed->Set(NanNew<String>("title"), NanNew<String>(title));
    }

    // Extracts the id property.

    char const *id = readTextNode(feedNode, "id");

    if (id) {

        feed->Set(NanNew<String>("id"), NanNew<String>(id));
    }

    // Extracts the link property.

    xml_node<char> *linkNode = feedNode->first_node("link");

    if (linkNode) {

        xml_attribute<char> *hrefAttr = linkNode->first_attribute("href");

        if (hrefAttr) {

            feed->Set(NanNew<String>("link"), NanNew<String>(hrefAttr->value()));
        }
    }

    // Extracts the author property.

    char const *author = readTextNode(feedNode, "author");

    if (author) {

        feed->Set(NanNew<String>("author"), NanNew<String>(author));
    }

    // Extract all channel items.

    Local<Array> items = NanNew<Array>();

    xml_node<char> *itemNode = feedNode->first_node("entry");

    int i = 0;

    while (itemNode) {

        Local<Object> item = NanNew<Object>();

        // Extracts the id property.

        char const *id = readTextNode(itemNode, "id");

        if (id) {

            item->Set(NanNew<String>("id"), NanNew<String>(id));
        }

        Local<Array> links = NanNew<Array>();

        // Extracts all links.
        // 4.2.7. The "atom:link" Element

        xml_node<char> *linkNode = itemNode->first_node("link");

        int linkIndex = 0;

        while (linkNode) {

            Local<Object> link = NanNew<Object>();

            xml_attribute<char> *relAttr = linkNode->first_attribute("rel");

            xml_attribute<char> *hrefAttr = linkNode->first_attribute("href");

            xml_attribute<char> *typeAttr = linkNode->first_attribute("type");

            xml_attribute<char> *hreflangAttr = linkNode->first_attribute("hreflang");

            xml_attribute<char> *titleAttr = linkNode->first_attribute("title");

            xml_attribute<char> *lengthAttr = linkNode->first_attribute("length");

            if (relAttr) {

                link->Set(NanNew<String>("rel"), NanNew<String>(relAttr->value()));
            }

            if (hrefAttr) {

                link->Set(NanNew<String>("href"), NanNew<String>(hrefAttr->value()));
            }

            if (typeAttr) {

                link->Set(NanNew<String>("type"), NanNew<String>(typeAttr->value()));
            }

            if (hreflangAttr) {

                link->Set(NanNew<String>("hreflang"), NanNew<String>(hreflangAttr->value()));
            }

            if (titleAttr) {

                link->Set(NanNew<String>("title"), NanNew<String>(titleAttr->value()));
            }

            if (lengthAttr) {

                link->Set(NanNew<String>("length"), NanNew<String>(lengthAttr->value()));
            }

            xml_node<char> *textNode = linkNode->first_node();

            // This is not by spec but some feeds
            // put URL/IRI into link's text node like:
            // <link>http://example.com</link>

            if (textNode) {

                link->Set(NanNew<String>("text"), NanNew<String>(textNode->value()));
            }

            links->Set(Number::New(linkIndex), link);

            linkIndex++;

            linkNode = linkNode->next_sibling("link");
        }

        item->Set(NanNew<String>("links"), links);

        // Extract the item title.

        char const *title = readTextNode(itemNode, "title");

        if (title) {

            item->Set(NanNew<String>("title"), NanNew<String>(title));
        }

        // Extract the published property.

        char const *date = readTextNode(itemNode, "published");

        if (date) {

            item->Set(NanNew<String>("date"), NanNew<String>(date));
        }

        // Extract the updated property.
        // Overwrites date set from published.

        date = readTextNode(itemNode, "updated");

        if (date) {

            item->Set(NanNew<String>("date"), NanNew<String>(date));
        }

        // Extract the item author.

        char const *author = readTextNode(itemNode, "author");

        if (author) {

            item->Set(NanNew<String>("author"), NanNew<String>(author));
        }

        if (extractContent) {

            // Extract the item summary.

            char const *summary = readTextNode(itemNode, "summary");

            if (summary) {

                item->Set(NanNew<String>("summary"), NanNew<String>(summary));
            }

            // Extract the item content.

            char const *content = readTextNode(itemNode, "content");

            if (content) {

                item->Set(NanNew<String>("content"), NanNew<String>(content));
            }
        }

        items->Set(NanNew<Number>(i), item);

        itemNode = itemNode->next_sibling("entry");

        i++;
    }

    feed->Set(NanNew<String>("items"), items);
}

// Parses the RSS feed.

void parseRssFeed(xml_node<char> *rssNode, const Local<Object> &feed, bool extractContent) {

    feed->Set(NanNew<String>("type"), NanNew<String>("rss"));

    xml_node<char> *channelNode = rssNode->first_node("channel");

    if (!channelNode) {

        NanThrowTypeError("Invalid RSS channel.");

        return;
    }

    // Extracts the title property.

    char const *title = readTextNode(channelNode, "title");

    if (title) {

        feed->Set(NanNew<String>("title"), NanNew<String>(title));
    }

    // Extracts the description property.

    char const *description = readTextNode(channelNode, "description");

    if (description) {

        feed->Set(NanNew<String>("description"), NanNew<String>(description));
    }

    // Extracts the link property.

    char const *link = readTextNode(channelNode, "link");

    if (link) {

        feed->Set(NanNew<String>("link"), NanNew<String>(link));
    }

    // Extracts the author property.

    char const *author = readTextNode(channelNode, "author");

    if (author) {

        feed->Set(NanNew<String>("author"), NanNew<String>(author));
    }

    // Extract all channel items.

    Local<Array> items = NanNew<Array>();

    xml_node<char> *itemNode = channelNode->first_node("item");

    int i = 0;
    while (itemNode) {

        Local<Object> item = NanNew<Object>();

        // Extracts the guid property.

        char const *guid = readTextNode(itemNode, "guid");

        if (guid) {

            item->Set(NanNew<String>("id"), NanNew<String>(guid));
        }

        // Extracts the link property.

        char const *link = readTextNode(itemNode, "link");

        if (link) {

            item->Set(NanNew<String>("link"), NanNew<String>(link));
        }

        // Extracts the pubDate property.

        char const *date = readTextNode(itemNode, "pubDate");

        if (date) {

            item->Set(NanNew<String>("date"), NanNew<String>(date));
        }

        // Sometimes given in Dublin Core extension.

        date = readTextNode(itemNode, "dc:date");

        if (date) {

            item->Set(NanNew<String>("date"), NanNew<String>(date));
        }

        // Extract the item title.

        char const *title = readTextNode(itemNode, "title");

        if (title) {

            item->Set(NanNew<String>("title"), NanNew<String>(title));
        }

        // Extract the item author.

        char const *author = readTextNode(itemNode, "author");

        if (author) {

            item->Set(NanNew<String>("author"), NanNew<String>(author));
        }

        if (extractContent) {

            // Extract the item description.

            char const *description = readTextNode(itemNode, "description");

            if (description) {

                item->Set(NanNew<String>("description"), NanNew<String>(description));
            }
        }

        items->Set(NanNew<Number>(i), item);

        itemNode = itemNode->next_sibling("item");

        i++;
    }

    feed->Set(NanNew<String>("items"), items);
}

NAN_METHOD(ParseFeed) {

    NanScope();

    if (args.Length() < 1) {

        NanThrowTypeError("Wrong number of arguments");

        NanReturnUndefined();
    }

    NanUtf8String xml(args[0]);

    xml_document<char> doc;

    try {

        doc.parse<0>(*xml);

    } catch(rapidxml::parse_error &e) {

        std::pair<int, int> loc = findErrorLine(*xml, e.where<char>());

        char buf[1024];

        snprintf(buf, 1024, "Error on line %d column %d: %s", loc.first, loc.second, e.what());

        NanThrowTypeError(buf);

        NanReturnUndefined();
    }

    bool extractContent = true;

    if (args.Length() == 2) {

        extractContent = args[1]->BooleanValue();
    }

    // Creates new object to store the feed
    // contents.

    Local<Object> feed = NanNew<Object>();

    // Tries to get either <rss> or <feed> node.

    xml_node<> *rssNode = doc.first_node("rss");

    if (rssNode) {

        parseRssFeed(rssNode, feed, extractContent);

    } else {

        xml_node<> *feedNode = doc.first_node("feed");

        if (feedNode) {

            parseAtomFeed(feedNode, feed, extractContent);

        } else {

            NanThrowTypeError("Invalid feed.");

            NanReturnUndefined();
        }
    }

    NanReturnValue(feed);
}

void init(Handle<Object> exports) {

  exports->Set(NanNew<String>("parse"),
      NanNew<FunctionTemplate>(ParseFeed)->GetFunction());
}

NODE_MODULE(parser, init)
