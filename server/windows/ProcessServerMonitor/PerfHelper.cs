using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ProcessServerMonitor
{
    /// <summary>
    /// Helpers class for performance counters.
    /// </summary>
    class PerfHelper
    {
        /// <summary>
        /// Creates the category for performance counters.
        /// </summary>
        /// <param name="category">Category.</param>
        /// <param name="categotyHelp">Category help.</param>
        /// <param name="type">SingleInstance or MultiInstance.</param>
        /// <param name="counterCreationDataCollection">In case of multiInstance
        /// we must provide a counter at the time of creation.</param>
        /// <returns>Category created or not.</returns>
        internal static bool CreateCategory(string category, string categotyHelp,
            PerformanceCounterCategoryType type,
            CounterCreationDataCollection counterCreationDataCollection)
        {
            if (!CategoryExists(category))
            {
                PerformanceCounterCategory.Create(category,
                    categotyHelp, type, counterCreationDataCollection);

                return (true);
            }
            else
            {
                Console.WriteLine("Category exists - {0}", category);
                return (false);
            }
        }

        /// <summary>
        /// Check if category exists.
        /// </summary>
        /// <param name="categoryName">Name.</param>
        /// <returns>If the category already exists.</returns>
        internal static bool CategoryExists(string categoryName)
        {
            return PerformanceCounterCategory.Exists(categoryName);
        }

    }
}
