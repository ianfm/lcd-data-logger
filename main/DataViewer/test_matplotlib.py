#!/usr/bin/env python3
"""
Test script to verify matplotlib backend compatibility
Run this before using pyviewer.py to check if your system supports interactive plotting
"""

import matplotlib
import matplotlib.pyplot as plt
import sys
import time

def test_matplotlib_backends():
    """Test different matplotlib backends for compatibility"""
    backends_to_test = ['TkAgg', 'Qt5Agg', 'Qt4Agg', 'Agg']
    working_backends = []
    
    print(f"Python version: {sys.version}")
    print(f"Matplotlib version: {matplotlib.__version__}")
    print("\nTesting matplotlib backends...")
    
    for backend in backends_to_test:
        try:
            matplotlib.use(backend, force=True)
            fig, ax = plt.subplots()
            ax.plot([1, 2, 3], [1, 4, 2])
            ax.set_title(f'Test plot with {backend} backend')
            
            if backend == 'Agg':
                plt.savefig(f'test_{backend}.png')
                print(f"✓ {backend}: Working (non-interactive, saves to file)")
                working_backends.append((backend, False))
            else:
                plt.ion()
                plt.show()
                plt.pause(0.1)
                plt.close()
                print(f"✓ {backend}: Working (interactive)")
                working_backends.append((backend, True))
                
        except Exception as e:
            print(f"✗ {backend}: Failed - {e}")
    
    return working_backends

def recommend_backend(working_backends):
    """Recommend the best backend to use"""
    if not working_backends:
        print("\n❌ ERROR: No working matplotlib backends found!")
        return None
    
    # Prefer interactive backends
    interactive_backends = [b for b in working_backends if b[1]]
    if interactive_backends:
        recommended = interactive_backends[0][0]
        print(f"\n✅ RECOMMENDED: Use {recommended} for interactive plotting")
        return recommended
    else:
        recommended = working_backends[0][0]
        print(f"\n⚠️  FALLBACK: Use {recommended} for file-based plotting (non-interactive)")
        return recommended

if __name__ == "__main__":
    print("Matplotlib Backend Compatibility Test")
    print("=" * 40)
    
    working = test_matplotlib_backends()
    recommended = recommend_backend(working)
    
    if recommended:
        print(f"\nThe pyviewer.py script should work with your system.")
        if recommended == 'Agg':
            print("Note: Plots will be saved as PNG files instead of interactive windows.")
    else:
        print(f"\n❌ Your system may have issues with matplotlib.")
        print("Try installing: pip install matplotlib tkinter")
