# purehtml

*purehtml*
* **is a reusable purely HTML-only parser** that can be used for developing
**crawlers**, **converters** or **simple clients** for consuming HTML.
* **aims to follow the latest [WHATWG HTML LS](https://html.spec.whatwg.org)**
specification.
* **is intentionally small / "human-scale"** so that it can be understood or
maintained by a single person without requiring a team.
* **allows using it in either in DOM or SAX mode**, that is either by immediately
working with the data we get and then release the data as we go (low processing
and memory footprint), or waiting for getting the full tree constructed so that
we can navigate the tree and then do whatever we want, for how many times we
like.

The parser can be used to create other programs that, for example:
* Programmatically read data from a HTML page.
* Convert HTML to XML.
* Convert HTML to text/gemini.
* Convert HTML to Markdown.
* Indent HTML.
* Minify HTML.
* Validate HTML.
* Make an RSS or Atom feed directly from HTML content.
* View HTML in a special way for increased accessibility (e.g. for blind users).
* Allow geeks to ultimately customize the Web user experience.

The parser intentionally does not support:
* **Javascript**: HTML is still somewhat "human-scale", but Javascript is not. Adding
Javascript would mean a team would need to support the parser, or understand its code.
There are other projects that require a team. Here, the purpose is to support
"human-scale" development.
* **CSS**: Parsing CSS is far simpler than Javascript and CSS support could be added,
but it is better for maintainability and for facilitating human-scale development
if *purehtml* is used for extracting the CSS content and then feeding
it to a separate parser. CSS is not needed by the most use cases envisioned for
*purehtml*, so bundling CSS with *purehtml* would be bloating the parser in most
use cases.
* **Full DOM**: Supporting full DOM as specified in
[WHATWG DOM Living Standard](https://dom.spec.whatwg.org) is not
necessary because there is no Javascript support. However, it is very easy to
use the parser either in a SAX or DOM-style.
* **JSON-LD**: JSON-LD content can be extracted using *purehtml* and then fed to a
separate parser. On the other hand, Microdata does not need a separate parser
but *purehtml* can do it because Microdata is HTML.

## About the intentional lack of Javascript support

Some Web sites are broken because they require Javascript. Javascript
should be used only for providing optional interactivity enhancements. Since
such sites are essentially broken (or at least they shouldn't be seen as
having anything to do with programmatically readable HTML), *purehtml* will
also fail with these.

However, being able to disable Javascript completely:
* greatly decreases interaction latency and speeds up page loading (which is important,
see [Response Times: The 3 Important Limits](https://www.nngroup.com/articles/response-times-3-important-limits/));
* facilitates human-scale development;
* hugely increases privacy;
* hugely decreases theoretical risk for security vulnerabilities because there is thousands times less attack surface to worry about;
* removes asynchronous page load behavior which is highly annoying and decreases
usability;
* moves control back from Web developers to application developers (e.g. developers
that use *purehtml* for developing customized special-purpose clients) and to
end users (because they can choose the application based on how they want to
view the content).

## Dependencies

None.

**The parser intentionally does not require libraries or infrastructure** other than
a normal near POSIX-compliant environment and a C compiler, so that using the
parser would be as simple as possible for the envisioned use cases.

## See also

* [QuickJS Javascript Engine](https://bellard.org/quickjs) is about 50,000 lines
of code Javascript engine which could be used for providing Javascript support,
but then we would need to support all the DOM operations and generally anything
that HTML5 conventionally requires from the clients, which is a huge lot of things.
* [Tidy-HTML5](https://github.com/htacg/tidy-html5) is about 50,000 lines of
code HTML5 parser, which is huge compared to *purehtml*, but it could be used
instead of *purehtml* if Javascript support would need to be integrated. 
* [edbrowse](https://edbrowse.org) Combining
of *Tidy-HTML5* and *QuickJS* is well demonstrated by *edbrowse*. The *edbrowse* is
originally written and intended for blind users, but similar customized view for
the blind users could be created using *purehtml* as the base.
