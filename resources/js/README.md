# JS rendering architecture in Liferea

Liferea does render 3 different types of content

1. 3rd party webpages (internal browser mode)
2. feed infos
3. item content

Type 2. and 3. are rendered with Javascript. Entrypoint is `htmlview.js`.

## Used technology

- Feed and item content is passed in JSON for rendering
- When passing data to JS using a Webkit Javascript call all JSON data
  is URI encoded
- Stripping (with the goal of security) is done with DOMPurify.js
- Stripping (with the goal of UX) is done with Readability.js
- CSP is applied to limit 3rd party resource loading
- Templating happens with Handlebars.js
- Handlebar templates (`item.xml.in` and `node.xml.in`) are C-runtime 
  preprocessed with XSLT to inject gettext translations using `i18n-filter.xslt`
- All ressources (handlebar templates as well as JS files) are 
  compiled as gresources
