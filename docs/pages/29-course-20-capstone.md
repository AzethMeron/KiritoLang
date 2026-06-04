# Lesson 20 — Capstone: A Contact Book

Time to bring it all together. In this final lesson we build a small but complete program — a
**contact book** that stores people, looks them up, and persists to a JSON file on disk. It uses
nearly everything from the course: classes, private state, exceptions, type annotations, files,
the `json` module, sorting with a key, and clean function composition.

Read it top to bottom; each piece builds on the last, and the whole thing runs as one program.

## The data: a `Contact`

A `Contact` is a small value type with a friendly `_str_` and a method to turn itself into a plain
Dict (so the `json` module can serialize it):

```kirito
var io = import("io")
var json = import("json")

class Contact:
    var _init_ = Function(self, name : String, email : String, phone : String):
        self.name = name
        self.email = email
        self.phone = phone

    var _str_ = Function(self) -> String:
        return f"{self.name} <{self.email}> {self.phone}"

    # Convert to a plain Dict for JSON storage.
    var to_dict = Function(self) -> Dict:
        return {"name": self.name, "email": self.email, "phone": self.phone}
```

## The errors

Specific exception types let callers react precisely. We define two:

```kirito
class DuplicateContact:
    var _init_ = Function(self, name):
        self.name = name

class ContactNotFound:
    var _init_ = Function(self, name):
        self.name = name
```

## The collection: a `ContactBook`

The book keeps contacts in a **private** Dict keyed by name, and exposes a clean interface. Notice
how each method has one job, validates its inputs, and throws a specific error when something's
wrong:

```kirito
class ContactBook:
    var _init_ = Function(self):
        self._by_name = {}              # private: name -> Contact

    var add = Function(self, contact : Contact):
        if contact.name in self._by_name:
            throw DuplicateContact(contact.name)
        self._by_name[contact.name] = contact

    var find = Function(self, name : String) -> Contact:
        if name in self._by_name:
            return self._by_name[name]
        throw ContactNotFound(name)

    var remove = Function(self, name : String):
        if name in self._by_name:
            self._by_name.remove(name)
        else:
            throw ContactNotFound(name)

    var count = Function(self) -> Integer:
        return len(self._by_name)

    # All contacts, sorted alphabetically by name (sorted() with a key function).
    var all_sorted = Function(self):
        return sorted(self._by_name.values(), key=Function(c): return c.name)
```

## Persistence: save and load JSON

Saving walks the contacts into a List of plain Dicts and writes JSON; loading parses it back and
rebuilds `Contact` instances. Both use a `with` block so the file is always closed:

```kirito
class ContactBook:
    var _init_ = Function(self):
        self._by_name = {}

    var add = Function(self, contact : Contact):
        if contact.name in self._by_name:
            throw DuplicateContact(contact.name)
        self._by_name[contact.name] = contact

    var find = Function(self, name : String) -> Contact:
        if name in self._by_name:
            return self._by_name[name]
        throw ContactNotFound(name)

    var remove = Function(self, name : String):
        if name in self._by_name:
            self._by_name.remove(name)
        else:
            throw ContactNotFound(name)

    var count = Function(self) -> Integer:
        return len(self._by_name)

    var all_sorted = Function(self):
        return sorted(self._by_name.values(), key=Function(c): return c.name)

    var save = Function(self, path : String):
        var records = []
        for contact in self.all_sorted():
            records.append(contact.to_dict())
        with io.open(path, "w") as f:
            f.write(json.dumps(records, indent=2))

    var load = Function(self, path : String):
        with io.open(path, "r") as f:
            var records = json.loads(f.read())
        for record in records:
            self.add(Contact(record["name"], record["email"], record["phone"]))
```

> We repeated the earlier methods here because each documentation block is a standalone program; in
> your own file you'd write the class once with all its methods together.

## The driver: exercising the program

Now we use the book — adding people, handling a duplicate, saving, loading into a fresh book, looking
someone up, and handling a missing name. Every failure mode is caught and reported, and the program
keeps running:

```kirito
var book = ContactBook()
book.add(Contact("Ada Lovelace", "ada@analytical.engine", "555-0100"))
book.add(Contact("Alan Turing", "alan@bombe.uk", "555-0011"))
book.add(Contact("Grace Hopper", "grace@cobol.navy", "555-0001"))

# Adding a duplicate is caught, not fatal.
try:
    book.add(Contact("Ada Lovelace", "dup@example.com", "555-9999"))
catch DuplicateContact as e:
    io.print(f"skipped duplicate: {e.name}")

io.print(f"book has {book.count()} contacts:")
for contact in book.all_sorted():
    io.print(f"  {contact}")            # uses Contact._str_

# Persist to disk and reload into a brand-new book.
var path = io.join(io.getcwd(), "contacts.json")
book.save(path)

var reloaded = ContactBook()
reloaded.load(path)
io.print(f"reloaded {reloaded.count()} contacts from disk")
var found = reloaded.find("Grace Hopper")
io.print(f"lookup: {found}")

# A missing lookup is handled gracefully.
try:
    discard reloaded.find("Nobody")
catch ContactNotFound as e:
    io.print(f"no contact named {e.name}")

discard io.remove(path)                 # clean up the demo file
```

Running this prints:

```text
skipped duplicate: Ada Lovelace
book has 3 contacts:
  Ada Lovelace <ada@analytical.engine> 555-0100
  Alan Turing <alan@bombe.uk> 555-0011
  Grace Hopper <grace@cobol.navy> 555-0001
reloaded 3 contacts from disk
lookup: Grace Hopper <grace@cobol.navy> 555-0001
no contact named Nobody
```

## Walkthrough — what each part of the course contributed

- **Classes (Lessons 13–14):** `Contact` bundles a person's data with a `_str_` so it prints itself;
  `ContactBook` bundles the storage with the operations on it.
- **Private state (13):** `_by_name` is private — callers go through `add`/`find`/`remove`, so the
  book stays consistent and you could change the storage later.
- **Exceptions (16):** `DuplicateContact` and `ContactNotFound` turn "bad input" into specific,
  catchable events; the driver recovers from each and carries on.
- **Annotations (12):** `: Contact`, `: String`, `-> Contact` document and enforce the contracts at
  each method boundary.
- **Files + JSON (18, 19):** `save`/`load` persist the whole book to human-readable JSON inside a
  `with` block that closes the file no matter what.
- **Functions & sorting (10, 7):** `all_sorted` returns contacts ordered by name using `sorted` with
  an inline key function.

## Where to go next

You now know enough to write real programs. To go further:

- **Extend this project:** add search by partial name, an `update` method, or load/save on startup
  and exit so the book persists across runs. Wrap it in an `io.input` loop for an interactive CLI.
- **Read the reference pages:** the **Standard Library**, **Built-in Functions**, and **Types**
  pages document every function and method with signatures.
- **Study real programs:** the `examples/` directory has complete projects — an RPN calculator, a
  word-frequency analyzer, a linear-system solver — and `examples/big_projects/` has larger ones
  (a networked SQL database, an HTTP server, a deep-learning library) written entirely in Kirito.
- **Embed or extend Kirito in C++:** the **Embedding** and **Extending** pages show how to run Kirito
  inside a C++ program and how to add your own native functions, modules, and types.

Congratulations — you've finished the **core course**. What follows are **bonus lessons**: deeper dives into specialized libraries — regular expressions, command-line programs, tabular data analysis, linear algebra, and tensors with automatic differentiation. Take them in any order, or go build something.
