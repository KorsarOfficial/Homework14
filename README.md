# Product & Category Management

## Overview

C++17 rewrite of a Python OOP coursework. Implements product inventory with statistical analysis using STL containers and algorithms. Single-file C++17, no external dependencies.

## Mathematical Formalization

- **Total value**: `V = Σᵢ (priceᵢ · qtyᵢ)`
- **Average price**: `μ = (1/n)·Σᵢ priceᵢ`
- **Price variance** (Welford's online algorithm, O(n)):
  - `M₁ = x₁`
  - `Mₖ = Mₖ₋₁ + (xₖ - Mₖ₋₁)/k`
  - `Sₖ = Sₖ₋₁ + (xₖ - Mₖ₋₁)·(xₖ - Mₖ)`
  - `σ² = Sₙ / n`
- **Median price**: `nth_element` partitioning, `T(n) = O(n)` average
- **Prefix search**: `sort + lower_bound → T(n log n + log n)`
- **Atomic counters**: `std::atomic<int>`, `memory_order_relaxed` for thread-safe instance/product counting

| Operation | Complexity |
|---|---|
| `total_value` | O(n) |
| `avg_price` | O(n) |
| `price_variance` | O(n) |
| `median_price` | O(n) average |
| `find_by_prefix` | O(n log n + log n) |
| `get_in_range` | O(n) |
| `sort_by_price` | O(n log n) |
| `min_price` / `max_price` | O(n) |

## Original Code Quality Analysis

The original Python implementation contains several critical bugs and design issues:

1. **`name = str` / `products = list` assigns the type object, not an annotation.** `category.name` would return `<class 'str'>`, not a string value. The correct syntax for a type annotation is `name: str`, not `name = str`. The `=` operator binds the built-in type constructor to the class variable.

2. **Shared mutable class variable `products = list`.** This is a classic Python pitfall: all instances of the class share the same list object reference. Adding a product to one category mutates the list for every category. The fix is `self.products = []` inside `__init__`, creating a per-instance list.

3. **`product_count += len(products)` counter bug.** `product_count` is declared as a class variable. After `__init__` executes `self.product_count += len(products)`, Python creates an instance variable that shadows the class variable. The class-level counter is never actually incremented, and each instance starts from the class variable's stale value.

4. **No methods beyond `__init__`.** Both `Product` and `Category` classes contain only `__init__`. There is no behavior, no encapsulation, no `__repr__`, no comparison operators, no validation. They are pure data bags with no domain logic — the class abstraction provides zero benefit over a plain dictionary or `namedtuple`.

5. **AI generation markers.** The code exhibits mechanical structure typical of AI-generated output: only `__init__` in both classes, type annotations written as assignments (`name = str` instead of `name: str`), no input validation, no `__repr__` or `__str__`, no operator overloads, no property decorators, and no methods that operate on the data.

## Build

```
g++ -std=c++17 -O2 -o hw14 main.cpp
```
