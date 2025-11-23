# inspect_vars.py
from db_variables import init_db, get_session, SystemVariable, VariableAccessPath
import json
import argparse

# ============================================================================
# Display helpers
# ============================================================================

def format_category(category: str) -> str:
    """Format category with color coding"""
    colors = {
        "config": "\033[94m",  # Blue
        "prot": "\033[93m",    # Yellow
        "oper": "\033[92m"     # Green
    }
    reset = "\033[0m"
    color = colors.get(category, "")
    return f"{color}{category:6s}{reset}"


def format_locator(locator: str) -> str:
    """Format locator, handle None"""
    if locator:
        return f"{locator:25s}"
    return f"{'(none)':25s}"


# ============================================================================
# Main inspection
# ============================================================================

def inspect_variables(filter_category=None, filter_locator=None, show_paths=True):
    """Inspect variables with optional filtering"""
    SessionLocal = get_session()
    
    with SessionLocal() as db:
        query = db.query(SystemVariable).order_by(SystemVariable.category, SystemVariable.name)
        
        # Apply filters
        if filter_category:
            query = query.filter(SystemVariable.category == filter_category)
        
        if filter_locator:
            if "[*]" in filter_locator:
                pattern = filter_locator.replace("[*]", "[%]")
                query = query.filter(SystemVariable.locator.like(pattern))
            else:
                query = query.filter(SystemVariable.locator == filter_locator)
        
        vars_ = query.all()
        
        print("=" * 120)
        print(f"{'Variable Name':<30s} {'Cat':<8s} {'Locator':<25s} {'Ver':<8s} {'Units':<10s} {'Description':<30s}")
        print("=" * 120)
        
        for v in vars_:
            cat_str = format_category(v.category)
            loc_str = format_locator(v.locator)
            ver_str = f"v{v.variable_version}/{v.system_version}"
            units_str = v.units[:10] if v.units else ""
            desc_str = v.description[:30] if v.description else ""
            
            print(f"{v.name:<30s} {cat_str} {loc_str} {ver_str:<8s} {units_str:<10s} {desc_str:<30s}")
            
            if show_paths:
                for p in v.access_paths:
                    if p.active:
                        ref = json.loads(p.reference)
                        table = ref.get("table", "")
                        offset = ref.get("offset", 0)
                        size = ref.get("size", 2)
                        num = ref.get("num", 1)
                        print(f"  └─> {p.access_type:<14s} table={table:<20s} offset={offset:<6d} size={size} num={num}")
        
        print("=" * 120)
        print(f"Total: {len(vars_)} variables")
        
        # Summary by category
        categories = {}
        for v in vars_:
            categories[v.category] = categories.get(v.category, 0) + 1
        
        print("\nSummary by category:")
        for cat, count in sorted(categories.items()):
            cat_str = format_category(cat)
            print(f"  {cat_str}: {count} variables")


def inspect_summary():
    """Show high-level summary"""
    SessionLocal = get_session()
    
    with SessionLocal() as db:
        total = db.query(SystemVariable).count()
        
        # Count by category
        categories = db.query(
            SystemVariable.category,
            db.query(SystemVariable).filter(SystemVariable.category == SystemVariable.category).count()
        ).distinct().all()
        
        # Get unique locators
        locators = db.query(SystemVariable.locator).distinct().all()
        locator_list = [loc[0] for loc in locators if loc[0]]
        
        print("=" * 60)
        print("Variable Registry Summary")
        print("=" * 60)
        print(f"Total variables: {total}")
        print()
        
        print("Categories:")
        for cat in ["config", "prot", "oper"]:
            count = db.query(SystemVariable).filter(SystemVariable.category == cat).count()
            cat_str = format_category(cat)
            print(f"  {cat_str}: {count}")
        
        print()
        print(f"Unique locators: {len(locator_list)}")
        for loc in sorted(locator_list):
            count = db.query(SystemVariable).filter(SystemVariable.locator == loc).count()
            print(f"  {loc:<30s}: {count} variables")
        
        print("=" * 60)


# ============================================================================
# Main entry point
# ============================================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Inspect Variable Registry')
    parser.add_argument('--category', type=str, default=None,
                        help='Filter by category (config, prot, oper)')
    parser.add_argument('--locator', type=str, default=None,
                        help='Filter by locator (supports wildcards with [*])')
    parser.add_argument('--no-paths', action='store_true',
                        help='Hide access paths')
    parser.add_argument('--summary', action='store_true',
                        help='Show summary only')
    
    args = parser.parse_args()
    
    # Initialize database
    init_db()
    
    if args.summary:
        inspect_summary()
    else:
        inspect_variables(
            filter_category=args.category,
            filter_locator=args.locator,
            show_paths=not args.no_paths
        )

# ============================================================================
# ## Features:

# ### 1. **Enhanced Display**
# - Shows category, locator, version, units, and description
# - Color-coded categories (config=blue, prot=yellow, oper=green)
# - Tree-style display for access paths

# ### 2. **Filtering Options**
# ```bash
# # Show all variables
# python3 inspect_vars.py

# # Show only protection variables
# python3 inspect_vars.py --category prot

# # Show only chiller variables
# python3 inspect_vars.py --locator "sbmu.chiller[n]"

# # Show all cell-related variables (wildcard)
# python3 inspect_vars.py --locator "rbms.cell[*]"

# # Show config variables without access paths
# python3 inspect_vars.py --category config --no-paths

# # Show summary only
# python3 inspect_vars.py --summary
# ```

# ### 3. **Summary Mode**
# Shows high-level statistics:
# - Total variable count
# - Count by category
# - Unique locators with variable counts

# ---

# ## Example Output:

# ### Full listing:
# ```
# ========================================================================================================================
# Variable Name                  Cat      Locator                   Ver      Units      Description                   
# ========================================================================================================================
# config_cfg                     config   system.global             v1/45              Imported from config:rack:sm16
# rs_485_1                       config   system.rs485[0]           v1/12              Imported from oper:rack:sm16  
# rs_485_2                       config   system.rs485[1]           v1/18              Imported from oper:rack:sm16  
#   └─> memory_offset  table=oper:rack:sm16      offset=7198   size=2 num=16
# cell_voltage_max_warn          prot     rbms.cell[n]              v1/23              Imported from prot:sbmu:input 
#   └─> memory_offset  table=prot:sbmu:input     offset=1000   size=2 num=1
# online                         oper     system.global             v1/3               Imported from oper:rack:sm16  
#   └─> memory_offset  table=oper:rack:sm16      offset=26626  size=2 num=1
# ========================================================================================================================
# Total: 42 variables

# Summary by category:
#   config: 3 variables
#   prot  : 35 variables
#   oper  : 4 variables
# ```

# ### Summary mode:
# ```
# ============================================================
# Variable Registry Summary
# ============================================================
# Total variables: 42

# Categories:
#   config: 3
#   prot  : 35
#   oper  : 4

# Unique locators: 5
#   rbms.cell[n]                  : 14 variables
#   rbms.module[n]                : 7 variables
#   sbmu.global                   : 8 variables
#   system.global                 : 10 variables
#   system.rs485[0]               : 1 variables
# ============================================================
# ```

# Much more useful for debugging and understanding your variable registry! 🎯