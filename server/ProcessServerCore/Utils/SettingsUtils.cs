using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Configuration;
using System.Diagnostics;
using System.Linq;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    public static class SettingsUtils
    {
        /// <summary>
        /// Initialize settings object
        /// </summary>
        /// <param name="settings">Settings object</param>
        /// <param name="freezeProperties">
        /// Should properties collection of object be read-only
        /// </param>
        /// <param name="arePropertyValuesReadOnly">
        /// If true, marks the value of each property in the
        /// object as read-only
        /// </param>
        public static void InitializeSettings(
            this SettingsBase settings, bool freezeProperties, bool arePropertyValuesReadOnly)
        {
            if (freezeProperties)
            {
                settings.Properties.SetReadOnly();
            }

            if (arePropertyValuesReadOnly)
            {
                foreach (SettingsProperty currProp in settings.Properties)
                {
                    // NOTE-SanKumar-1902: Even though set, it's not honourned by
                    // .Net (Refered source code)
                    //currProp.ThrowOnErrorDeserializing = true;

                    // TODO-SanKumar-1906: Currently this is applied to all the
                    // settings. But we could set separate flags to make user and
                    // app settings as readonly.
                    currProp.IsReadOnly = true;

                    // TODO-SanKumar-1906: Force the reload and subscribe to
                    // settings loaded event. Then enumerate all the properties
                    // to see if all of them are loading correctly.
                    // Or, let the parsing happen on each of the configuration
                    // by its own.
                    // object temp = this[currProp.Name];
                }
            }
        }
    }

    public class TunablesBasedSettingsProvider : LocalFileSettingsProvider
    {
        private readonly object m_loadLock = new object();
        private volatile IReadOnlyDictionary<string, string> m_tunables;

        public override void Initialize(string name, NameValueCollection values)
        {
            base.Initialize(name, values);

            var latestTunables = CSApi.ProcessServerSettings.GetCachedSettings()?.Tunables;

            lock (m_loadLock)
            {
                // First time load tunables
                m_tunables = latestTunables;
            }
        }

        public static void OnTunablesChanged(
            IReadOnlyDictionary<string, string> latestTunables,
            params ApplicationSettingsBase[] settings)
        {
            if (settings == null)
                throw new ArgumentNullException(nameof(settings));

            foreach (var currSetting in settings)
            {
                try
                {
                    var tunableSettingsProv = currSetting.Providers.Cast<TunablesBasedSettingsProvider>().Single();
                    if (tunableSettingsProv.UpdateTunables(latestTunables))
                    {
                        currSetting.Reload();
                    }
                }
                catch (Exception ex)
                {
                    Tracers.Misc.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to update tunables and reload setting - {0}{1}{2}",
                        currSetting.GetType(), Environment.NewLine, ex);
                }
            }
        }

        private bool UpdateTunables(IReadOnlyDictionary<string, string> latestTunables)
        {
            bool UpdateAndNotifyToReload()
            {
                m_tunables = latestTunables;
                return true;
            }

            lock (m_loadLock)
            {
                // Compare dictionaries and notify to reload on mismatch

                if (m_tunables == null && latestTunables == null)
                    return false;

                if (m_tunables == null || latestTunables == null)
                    return UpdateAndNotifyToReload();

                if (m_tunables.Count != latestTunables.Count)
                    return UpdateAndNotifyToReload();

                foreach (var currTunable in m_tunables)
                {
                    if (!latestTunables.TryGetValue(currTunable.Key, out string latestTunableValue))
                        return UpdateAndNotifyToReload();

                    if (!string.Equals(currTunable.Value, latestTunableValue, StringComparison.Ordinal))
                        return UpdateAndNotifyToReload();
                }
            }

            return false;
        }

        public override SettingsPropertyValueCollection GetPropertyValues(
            SettingsContext context,
            SettingsPropertyCollection properties)
        {
            var propValColl = base.GetPropertyValues(context, properties);

            lock (m_loadLock)
            {
                if (m_tunables != null && m_tunables.Count > 0)
                {
                    foreach (SettingsPropertyValue currPropVal in propValColl)
                    {
                        // If a value is set on the local app.config file, it
                        // takes precedence over the tunable.
                        if (!string.Equals(
                            (string)currPropVal.SerializedValue,
                            (string)currPropVal.Property.DefaultValue,
                            StringComparison.Ordinal))
                        {
                            // NOTE-SanKumar-1903: The shortcoming of this technique is when the local
                            // setting is provided in the app.config file but same as default value.
                            // This scenario may be needed, when we don't wanna use the Rcm provided
                            // tunable, so we want to instruct to override with the default value.
                            // Since the base class doesn't provide a clear distinction on if the
                            // value is picked from the hardcoded default or the file, we had to go
                            // with this approach of equivality comparison.
                            // currPropVal.UsingDefaultValue is not always correct.

                            continue;
                        }

                        // Otherwise, if the tunables from the server contains this property name,
                        // load its value.
                        if (m_tunables.TryGetValue(currPropVal.Name, out string tunableValue))
                        {
                            Debug.Assert(!currPropVal.Deserialized);

                            currPropVal.SerializedValue = tunableValue;
                        }
                    }
                }
            }

            return propValColl;
        }
    }
}
